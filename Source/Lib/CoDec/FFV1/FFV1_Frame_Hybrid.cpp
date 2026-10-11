/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Frame_Hybrid.h"
#ifdef RAWCOOKED_FFMPEG
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/CoDec/FFV1/FFV1_Frame_FFmpeg.h"
#include "Lib/CoDec/FFmpeg_Functions.h"
#include "Lib/CoDec/Wrapper.h"
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "ThreadPool.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
extern const state_transitions_struct default_state_transitions;

//---------------------------------------------------------------------------
// Decoders, the last available one is preferred for a keyframe
enum decoder_index : size_t
{
    Decoder_Ours,
    Decoder_FFmpeg,
    Decoder_Vulkan, // FFmpeg decoder with Vulkan hardware decoding
    Decoder_Max
};

//---------------------------------------------------------------------------
// Lower priority for the threads of our decoder, so the threads feeding FFmpeg decoders and the output threads are not delayed by our decoder
static void LowPriority()
{
    #if defined(_WIN32)
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    #elif defined(__linux__)
        setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 10);
    #elif defined(__APPLE__)
        pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
    #endif
}

//---------------------------------------------------------------------------
struct hybrid_job
{
    AVBufferRef*                Packet = nullptr; // Frame data with FFmpeg padding, shared with FFmpeg decoders
    size_t                      Packet_Size = 0;
    raw_frame*                  Frame = nullptr; // Decoded frame
    raw_frame_ready*            FrameReady = nullptr;
    void*                       Opaque = nullptr;
    bool                        Error = false;
    bool                        WarmUp = false; // Not output, for the initialization of an FFmpeg decoder
    bool                        First = false; // First frame for an FFmpeg decoder, decoded by the fallback decoder if this decoder fails
};

//---------------------------------------------------------------------------
struct ffv1_frame_hybrid_private
{
    // Decoders, each one in its own thread with its own thread pool (our decoder: low priority, FFmpeg decoders: transfer to raw frames, else their tasks would wait for the many tasks of our decoder)
    bool                        Used[Decoder_Max] = {};
    size_t                      Fallback = Decoder_Max; // First used decoder (our decoder if used), decodes the first frames that other decoders fail to decode
    ThreadPool*                 Pools[Decoder_Max] = {};
    ffv1_frame*                 Ours = nullptr;
    ffv1_frame_ffmpeg*          FFmpeg[Decoder_Max] = {}; // FFmpeg decoders, each one created in its thread (long initialization)
    uint32_t                    Width = 0;
    uint32_t                    Height = 0;
    std::vector<uint8_t>        Extradata;
    std::thread                 Threads[Decoder_Max];
    std::thread                 Transfer_Threads[Decoder_Max]; // Frames decoded by FFmpeg decoders to raw frames
    hybrid_job*                 Next[Decoder_Max] = {}; // Job to decode
    bool                        Busy[Decoder_Max] = {}; // Stays busy if the decoder is not used
    bool                        Working[Decoder_Max] = {}; // Decoding a frame
    bool                        Pending[Decoder_Max] = {}; // Decoder other than the fallback decoder without frame yet, its first frame is a warm-up or a first frame
    bool                        Disabled[Decoder_Max] = {}; // Decoder can not decode the stream
    double                      Duration[Decoder_Max] = {}; // Average duration of the decoding of a frame, in seconds
    bool                        Ending = false; // No more frames
    size_t                      Last = Decoder_Max; // Decoder of the previous frame
    bool                        Intra = false; // All frames are keyframes
    std::deque<hybrid_job*>     Redo; // First frames for FFmpeg decoders which failed, to be decoded by the fallback decoder
    size_t                      First_Pending = 0; // Count of first frames for FFmpeg decoders not decoded, the fallback decoder may have to decode them

    // Jobs
    std::deque<hybrid_job*>     Transfer_Jobs[Decoder_Max]; // Decoded by FFmpeg decoders, in decoding order
    std::vector<hybrid_job*>    Jobs_Free; // For reuse

    // Synchronization
    bool                        Stop = false;
    std::mutex                  Mutex;
    std::condition_variable     Condition;

    // Keyframe detection
    rangecoder                  E;

    ffv1_frame_hybrid_private(ThreadPool* Pool, bool UseOurDecoder, bool UseFFmpegDecoder, bool UseVulkanDecoder)
    {
        Used[Decoder_Ours] = UseOurDecoder;
        Used[Decoder_FFmpeg] = UseFFmpegDecoder;
        Used[Decoder_Vulkan] = UseVulkanDecoder;
        auto Count = std::thread::hardware_concurrency();
        if (!Count)
            Count = 4;
        for (size_t i = 0; i < Decoder_Max; i++)
        {
            if (!Used[i])
            {
                Busy[i] = true;
                continue;
            }
            if (Fallback == Decoder_Max)
                Fallback = i;
            else
                Pending[i] = true;
            if (Pool)
            {
                Pools[i] = new ThreadPool(Count, i == Decoder_Ours ? LowPriority : nullptr);
                Pools[i]->init();
            }
        }
        if (UseOurDecoder)
            Ours = new ffv1_frame(Pools[Decoder_Ours]);
        Last = Fallback;
        E.AssignStateTransitions(default_state_transitions);
    }

    ~ffv1_frame_hybrid_private()
    {
        for (auto Decoder : FFmpeg)
            delete Decoder;
        delete Ours;
        for (auto Pool : Pools)
            if (Pool)
            {
                Pool->shutdown();
                delete Pool;
            }
    }

    void Run(size_t i);
    void Transfer(size_t i);
    hybrid_job* Job_New();
    void Job_Free(hybrid_job* Job);
};

//---------------------------------------------------------------------------
// Decoder thread
void ffv1_frame_hybrid_private::Run(size_t i)
{
    if (i == Decoder_Ours)
        LowPriority();

    std::unique_lock<std::mutex> Lock(Mutex);
    for (;;)
    {
        Condition.wait(Lock, [&] { return Next[i] || (i == Fallback && !Redo.empty()) || Stop || (i == Decoder_Ours && Ending && Ours && !First_Pending); });
        auto Job = Next[i];
        if (Job)
            Next[i] = nullptr;
        else if (i == Fallback && !Redo.empty())
        {
            Job = Redo.front();
            Redo.pop_front();
        }
        if (!Job)
        {
            if (Stop)
                return;

            // No more frames, our decoder is released during the output of last frames
            auto Ours_Old = Ours;
            auto Pool_Old = Pools[i];
            Ours = nullptr;
            Pools[i] = nullptr;
            Lock.unlock();
            delete Ours_Old;
            if (Pool_Old)
            {
                Pool_Old->shutdown();
                delete Pool_Old;
            }
            Lock.lock();
            continue;
        }
        Working[i] = true;
        Lock.unlock();

        bool Error;
        auto Start = std::chrono::steady_clock::now();
        if (i == Decoder_Ours)
        {
            Ours->RawFrame = Job->Frame;
            Ours->Process(Job->Packet->data, Job->Packet_Size, [&Error](bool Ours_Error) { Error = Ours_Error; });
            Ours->Wait(); // End of the decoding of the frame
            ffmpeg::av_buffer_unref(&Job->Packet);
            Job->FrameReady->Ready(Job->Opaque, Error);
        }
        else
        {
            auto& Decoder = FFmpeg[i];
            if (!Decoder)
            {
                Decoder = new ffv1_frame_ffmpeg(Pools[i], i == Decoder_Vulkan);
                Decoder->SetWidth(Width);
                Decoder->SetHeight(Height);
                if (!Extradata.empty())
                    Decoder->OutOfBand(Extradata.data(), Extradata.size());
            }
            Error = Decoder->Decode(Job->Packet, Job->Packet_Size);
            if (Job->WarmUp || (Job->First && Error))
                Decoder->Discard(); // Not output
            if (!Job->First || !Error)
                ffmpeg::av_buffer_unref(&Job->Packet); // FFmpeg has its own reference if needed, else the packet is kept for the fallback decoder
        }
        auto Job_Duration = std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();

        Lock.lock();
        Working[i] = false;
        if (Job->First)
            First_Pending--;
        if (!Job->WarmUp && !Job->First)
            Duration[i] = Duration[i] ? (Duration[i] * 3 + Job_Duration) / 4 : Job_Duration; // Duration of the first frame includes the initialization
        if (i == Decoder_Ours)
        {
            Job_Free(Job);
            Busy[i] = !Redo.empty(); // Fallback decoder is not available for a new frame until the first frames for FFmpeg decoders which failed are decoded
        }
        else if (Job->First && Error)
        {
            // FFmpeg decoder can not decode the stream, the frame is decoded by the fallback decoder
            Disabled[i] = true; // FFmpeg decoder stays busy
            Job->First = false;
            Redo.push_back(Job);
            Busy[Fallback] = true;
        }
        else if (Job->WarmUp)
        {
            Job_Free(Job);
            if (Error)
                Disabled[i] = true; // FFmpeg decoder stays busy
            else
                Busy[i] = false;
        }
        else
        {
            Job->Error = Error;
            Transfer_Jobs[i].push_back(Job);
            Busy[i] = i == Fallback && !Redo.empty(); // Fallback decoder is not available for a new frame until the first frames for other FFmpeg decoders which failed are decoded
        }
        Condition.notify_all();
    }
}

//---------------------------------------------------------------------------
// Transfer thread, frames decoded by an FFmpeg decoder to raw frames
void ffv1_frame_hybrid_private::Transfer(size_t i)
{
    auto& Jobs = Transfer_Jobs[i];
    std::unique_lock<std::mutex> Lock(Mutex);
    for (;;)
    {
        Condition.wait(Lock, [&] { return !Jobs.empty() || Stop || (Ending && !Next[i] && !Working[i] && (i != Fallback || (Redo.empty() && !First_Pending)) && FFmpeg[i]); });
        if (Jobs.empty())
        {
            if (Stop)
                return;

            // No more frames, FFmpeg decoder is released during the output of last frames
            auto Decoder_Old = FFmpeg[i];
            auto Pool_Old = Pools[i];
            FFmpeg[i] = nullptr;
            Pools[i] = nullptr;
            Lock.unlock();
            delete Decoder_Old;
            if (Pool_Old)
            {
                Pool_Old->shutdown();
                delete Pool_Old;
            }
            Lock.lock();
            continue;
        }
        auto Job = Jobs.front();
        Jobs.pop_front();
        auto Decoder = FFmpeg[i];
        Lock.unlock();

        Decoder->RawFrame = Job->Frame;
        auto Error = Decoder->Output() || Job->Error;
        Job->FrameReady->Ready(Job->Opaque, Error);

        Lock.lock();
        Job_Free(Job);
    }
}

//---------------------------------------------------------------------------
hybrid_job* ffv1_frame_hybrid_private::Job_New()
{
    std::lock_guard<std::mutex> Lock(Mutex);
    if (Jobs_Free.empty())
        return new hybrid_job;
    auto Job = Jobs_Free.back();
    Jobs_Free.pop_back();
    return Job;
}

//---------------------------------------------------------------------------
// Mutex must be locked
void ffv1_frame_hybrid_private::Job_Free(hybrid_job* Job)
{
    Job->Frame = nullptr;
    Job->FrameReady = nullptr;
    Job->Opaque = nullptr;
    Job->Error = false;
    Job->WarmUp = false;
    Job->First = false;
    Jobs_Free.push_back(Job);
}

//---------------------------------------------------------------------------
static void Packet_Release(void* Holder, uint8_t*)
{
    ((data_holder*)Holder)->Release();
}

//---------------------------------------------------------------------------
// Frame data kept as long as the decoders need it, with FFmpeg padding, in allocated memory if Vulkan imports it in GPU memory space
static AVBufferRef* Packet_New(size_t Buffer_Size, data_holder* Holder, bool Allocated)
{
    auto Data = Holder->Hold(Allocated);
    auto Packet = ffmpeg::av_buffer_create(const_cast<uint8_t*>(Data), Buffer_Size + AV_INPUT_BUFFER_PADDING_SIZE, Packet_Release, Holder, AV_BUFFER_FLAG_READONLY);
    if (!Packet)
        Holder->Release();
    return Packet;
}

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
ffv1_frame_hybrid::ffv1_frame_hybrid(ThreadPool* Pool, bool UseOurDecoder, bool UseFFmpegDecoder, bool UseVulkanDecoder) :
    RawFrame(nullptr),
    FrameReady(nullptr),
    Remaining((size_t)-1),
    P(new ffv1_frame_hybrid_private(Pool, UseOurDecoder, UseFFmpegDecoder, UseVulkanDecoder))
{
    for (size_t i = 0; i < Decoder_Max; i++)
        if (P->Used[i])
        {
            P->Threads[i] = std::thread(&ffv1_frame_hybrid_private::Run, P, i);
            if (i != Decoder_Ours)
                P->Transfer_Threads[i] = std::thread(&ffv1_frame_hybrid_private::Transfer, P, i);
        }
}

//---------------------------------------------------------------------------
ffv1_frame_hybrid::~ffv1_frame_hybrid()
{
    // End of threads, after the end of their current job (the caller waited for the decoded frames)
    {
        std::lock_guard<std::mutex> Lock(P->Mutex);
        P->Stop = true;
    }
    P->Condition.notify_all();
    for (auto Threads : { P->Threads, P->Transfer_Threads })
        for (size_t i = 0; i < Decoder_Max; i++)
            if (Threads[i].joinable())
                Threads[i].join();

    for (auto Job : P->Jobs_Free)
        delete Job;
    delete P;
}

//***************************************************************************
// External metadata
//***************************************************************************

//---------------------------------------------------------------------------
void ffv1_frame_hybrid::SetWidth(uint32_t width)
{
    if (P->Ours)
        P->Ours->SetWidth(width);
    P->Width = width;
}

//---------------------------------------------------------------------------
void ffv1_frame_hybrid::SetHeight(uint32_t height)
{
    if (P->Ours)
        P->Ours->SetHeight(height);
    P->Height = height;
}

//***************************************************************************
// Actions
//***************************************************************************

//---------------------------------------------------------------------------
bool ffv1_frame_hybrid::OutOfBand(const uint8_t* Buffer, size_t Buffer_Size)
{
    auto Result = false;
    if (P->Ours)
    {
        P->Ours->RawFrame = RawFrame;
        Result = P->Ours->OutOfBand(Buffer, Buffer_Size);
        P->Intra = P->Ours->Intra();
    }
    else
    {
        // Parsing by our decoder (not used for decoding), for knowing if all frames are keyframes
        ffv1_frame Parser(nullptr);
        Parser.OutOfBand(Buffer, Buffer_Size);
        P->Intra = Parser.Intra();
    }
    P->Extradata.assign(Buffer, Buffer + Buffer_Size);
    return Result;
}

//---------------------------------------------------------------------------
size_t ffv1_frame_hybrid::Padding()
{
    return ffv1_frame_ffmpeg::Padding();
}

//---------------------------------------------------------------------------
void ffv1_frame_hybrid::Decode(const uint8_t* Buffer, size_t Buffer_Size, data_holder* Holder, raw_frame* Frame, void* Opaque)
{
    // A keyframe can be decoded by any decoder, other frames need the decoding states of the previous frame
    P->E.AssignBuffer(Buffer, Buffer_Size);
    uint8_t State = states_default;
    auto KeyFrame = P->E.b(State);

    // Job, with the frame data
    auto Packet = Packet_New(Buffer_Size, Holder, P->Used[Decoder_Vulkan]);
    if (!Packet)
    {
        FrameReady->Ready(Opaque, true);
        return;
    }
    auto Job = P->Job_New();
    Job->Packet = Packet;
    Job->Packet_Size = Buffer_Size;
    Job->Frame = Frame;
    Job->FrameReady = FrameReady;
    Job->Opaque = Opaque;

    // First frame for each FFmpeg decoder other than the fallback decoder, if all frames are keyframes: a keyframe decoded by this FFmpeg decoder during its initialization (long) while other decoders decode next frames,
    // else (the fallback decoder may have to decode this frame before next frames) first keyframe decoded by the fallback decoder and also by these FFmpeg decoders for their initialization without delaying the output
    hybrid_job* WarmUp_Jobs[Decoder_Max] = {};
    auto WarmUp = false;
    if (KeyFrame && !P->Intra)
        for (size_t i = 0; i < Decoder_Max; i++)
            if (P->Pending[i])
            {
                P->Pending[i] = false;
                if (auto WarmUp_Packet = ffmpeg::av_buffer_ref(Packet))
                {
                    auto WarmUp_Job = P->Job_New();
                    WarmUp_Job->Packet = WarmUp_Packet;
                    WarmUp_Job->Packet_Size = Buffer_Size;
                    WarmUp_Job->WarmUp = true;
                    WarmUp_Jobs[i] = WarmUp_Job;
                    WarmUp = true;
                }
            }

    // At the end of the stream, a decoder is not used if another decoder would decode this frame and the next frames before this decoder decodes this frame
    auto Remaining_ = Remaining;
    auto Allowed = [&](size_t i)
    {
        if (Remaining_ == (size_t)-1 || !P->Duration[i])
            return true;
        for (size_t j = 0; j < Decoder_Max; j++)
            if (j != i && !P->Disabled[j] && P->Duration[j] && P->Duration[i] >= (Remaining_ + 2) * P->Duration[j]) // Other decoder: end of the current frame, this frame, next frames
                return false;
        return true;
    };

    // Decoder of the previous frame if it is not a keyframe, fallback decoder if FFmpeg decoders warm up, else first available decoder (Vulkan decoder first, then FFmpeg decoder, then our decoder)
    std::unique_lock<std::mutex> Lock(P->Mutex);
    size_t i = Decoder_Max;
    P->Condition.wait(Lock, [&]
    {
        if (WarmUp)
        {
            for (size_t j = 0; j < Decoder_Max; j++)
                if (WarmUp_Jobs[j] && P->Busy[j])
                    return false;
            i = P->Fallback;
        }
        else if (!KeyFrame)
            i = P->Last;
        else
        {
            for (size_t j = Decoder_Max; j-- > 0;)
                if (!P->Busy[j] && Allowed(j))
                {
                    i = j;
                    return true;
                }
            return false;
        }
        return !P->Busy[i];
    });
    if (P->Pending[i])
    {
        // First frame for this FFmpeg decoder (all frames are keyframes)
        P->Pending[i] = false;
        Job->First = true;
        P->First_Pending++;
    }
    for (size_t j = 0; j < Decoder_Max; j++)
        if (WarmUp_Jobs[j])
        {
            P->Busy[j] = true;
            P->Next[j] = WarmUp_Jobs[j];
        }
    P->Busy[i] = true;
    P->Next[i] = Job;
    P->Last = i;
    P->Condition.notify_all();
}

//---------------------------------------------------------------------------
size_t ffv1_frame_hybrid::Depth()
{
    return 8; // Several frames from the faster decoders during the decoding of a frame by the slower decoder
}

//---------------------------------------------------------------------------
void ffv1_frame_hybrid::End()
{
    {
        std::lock_guard<std::mutex> Lock(P->Mutex);
        P->Ending = true;
    }
    P->Condition.notify_all();
}

#endif //RAWCOOKED_FFMPEG
