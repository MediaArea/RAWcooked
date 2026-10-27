/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "ThreadPool.h"
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "Lib/Utils/CRC32/ZenCRC32.h"
#include "Lib/Utils/Trace/Trace.h"
#include <atomic>
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreorder"
#endif
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
//---------------------------------------------------------------------------

//***************************************************************************
// Frames being decoded
//***************************************************************************

const size_t Slots_Count = 2; // End of a frame and begin of the next frame

struct ffv1_frame_slot
{
    slice_struct*               Slices = nullptr;
    size_t                      Slices_Count = 0; // Allocated
    raw_frame*                  Frame = nullptr;
    std::function<void(bool)>   Done;
    uint64_t                    Trace_Begin = 0; // Begin of the decoding, for instrumentation
    size_t                      Count = 0; // Slices of the frame
    std::atomic<size_t>         Started{ 0 };
    std::atomic<size_t>         Finished{ 0 };
    bool                        Busy = false; // From the slot selection to the end of the decoding
};

//***************************************************************************
// Info
//***************************************************************************

extern const state_transitions_struct default_state_transitions =
{
    {
      0,  0,  0,  0,  0,  0,  0,  0, 20, 21, 22, 23, 24, 25, 26, 27,
     28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40, 41, 42,
     43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 56, 57,
     58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
     74, 75, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
     89, 90, 91, 92, 93, 94, 94, 95, 96, 97, 98, 99,100,101,102,103,
    104,105,106,107,108,109,110,111,112,113,114,114,115,116,117,118,
    119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,133,
    134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,
    150,151,152,152,153,154,155,156,157,158,159,160,161,162,163,164,
    165,166,167,168,169,170,171,171,172,173,174,175,176,177,178,179,
    180,181,182,183,184,185,186,187,188,189,190,190,191,192,194,194,
    195,196,197,198,199,200,201,202,202,204,205,206,207,208,209,209,
    210,211,212,213,215,215,216,217,218,219,220,220,222,223,224,225,
    226,227,227,229,229,230,231,232,234,234,235,236,237,238,239,240,
    241,242,243,244,245,246,247,248,248,  0,  0,  0,  0,  0,  0,  0,
    }
};

static inline uint32_t BigEndian2int24u(const uint8_t* B)
{
    return (((uint32_t)B[0]) << 16) | (((uint32_t)B[1]) << 8) | B[2];
}

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
ffv1_frame::ffv1_frame(ThreadPool* Pool_) :
    // Decoded frame
    RawFrame(NULL),
    // Temp
    KeyFrame_IsPresent(false),
    Pool(Pool_),
    Slots(new ffv1_frame_slot[Slots_Count])
{
    E.AssignStateTransitions(default_state_transitions);
}

//---------------------------------------------------------------------------
ffv1_frame::~ffv1_frame()
{
    Clear();
    delete[] Slots;
}

//***************************************************************************
// External metadata
//***************************************************************************

//---------------------------------------------------------------------------
void ffv1_frame::SetWidth(uint32_t width)
{
    P.width = width;
}

//---------------------------------------------------------------------------
void ffv1_frame::SetHeight(uint32_t height)
{
    P.height = height;
}

//***************************************************************************
// Before - Global
//***************************************************************************

//---------------------------------------------------------------------------
bool ffv1_frame::OutOfBand(const uint8_t* Buffer, size_t Buffer_Size)
{
    if (!Buffer_Size)
        return true;

    P.ConfigurationRecord_IsPresent = true;
    Clear();

    //Coherency tests
    if (Buffer_Size<4)
        return P.Error("FFV1-HEADER-END:1");
    if (ZenCRC32(Buffer, (size_t)Buffer_Size))
        return P.Error("FFV1-HEADER-configuration_record_crc_parity:1");

    E.AssignBuffer(Buffer, Buffer_Size-4);

    if (P.Parse(E, true))
        return true;

    // Slices
    for (size_t s = 0; s < Slots_Count; s++)
    {
        auto& Slot = Slots[s];
        Slot.Slices_Count = P.num_h_slices * P.num_v_slices;
        Slot.Slices = new slice_struct[Slot.Slices_Count];
        memset(Slot.Slices, 0x00, Slot.Slices_Count * sizeof(slice_struct));
    }

    // Reserved + configuration_record_crc_parity

    return true;
}

//---------------------------------------------------------------------------
void ffv1_frame::Process(const uint8_t* Buffer, size_t Buffer_Size, std::function<void(bool Error)> Done)
{
    if (P.ConfigurationRecord_IsPresent && !Slots[0].Slices)
    {
        Done(true); //There is a problem
        return;
    }

    E.AssignBuffer(Buffer, Buffer_Size);

    // keyframe
    uint8_t State = states_default;
    bool keyframe = E.b(State);
    if (P.ConfigurationRecord_IsPresent && P.intra && !keyframe)
    {
        P.Error("FFV1-FRAME-key_frame-ISNOTINTRA:1");
        Done(true);
        return;
    }
    if (keyframe)
        KeyFrame_IsPresent = true;
    if (!KeyFrame_IsPresent)
    {
        P.Error("FFV1-FRAME-key_frame-NOINFIRSTFRAME:1");
        Done(true);
        return;
    }

    // Slot: a keyframe uses a free slot, other frames use the slot of the previous frame (decoding states of the previous frame),
    // all previous frames are decoded if there is no configuration record (parameters are parsed by the slice)
    ffv1_frame_slot* Slot;
    auto Trace_Begin = trace::Enabled() ? trace::Now() : 0;
    {
        trace::scope Trace_Wait("Ours slot wait", RawFrame->Index);
        std::unique_lock<std::mutex> Lock(Mutex);
        auto s = Slot_Last;
        if (P.ConfigurationRecord_IsPresent && keyframe)
        {
            Condition.wait(Lock, [&]
            {
                for (size_t i = 1; i <= Slots_Count; i++)
                {
                    s = (Slot_Last + i) % Slots_Count;
                    if (!Slots[s].Busy)
                        return true;
                }
                return false;
            });
        }
        else
        {
            Condition.wait(Lock, [&]
            {
                for (size_t i = 0; i < Slots_Count; i++)
                    if (Slots[i].Busy && (i == Slot_Last || !P.ConfigurationRecord_IsPresent))
                        return false;
                return true;
            });
        }
        Slot = &Slots[s];
        Slot->Busy = true;
        Slot_Last = s;
        trace::Counter("Ours frames", (int64_t)Slots_Busy());
    }
    Slot->Frame = RawFrame;
    Slot->Done = std::move(Done);
    Slot->Trace_Begin = Trace_Begin;
    auto Abort = [&]()
    {
        Done = std::move(Slot->Done);
        {
            std::lock_guard<std::mutex> Lock(Mutex);
            Slot->Busy = false;
            trace::Counter("Ours frames", (int64_t)Slots_Busy());
        }
        Condition.notify_all();
        Done(true);
    };

    if (!Slot->Slices)
    {
        // No slice in this stream, fake 1 slice
        Slot->Slices_Count = 1;
        Slot->Slices = new slice_struct[1];
        memset(Slot->Slices, 0x00, sizeof(slice_struct));
    }
    auto Slices = Slot->Slices;

    size_t Slices_Size = 0;
    if (P.ConfigurationRecord_IsPresent)
    {
        if (P.num_h_slices > P.width) // Up to 1 pixel per slice, e.g. FFmpeg Vulkan encoder with small frames
        {
            P.Error("FFV1-HEADER-num_h_slices:1");
            Abort();
            return;
        }
        if (P.num_v_slices > P.height)
        {
            P.Error("FFV1-HEADER-num_v_slices:1");
            Abort();
            return;
        }

        size_t info = 0
        | ((P.bits_per_raw_sample - 1)          <<  0)
        | (P.chroma_planes                      <<  6)
        | (P.alpha_plane                        <<  7)
        | ((1 << P.log2_h_chroma_subsample)     <<  8)
        | ((1 << P.log2_v_chroma_subsample)     << 12)
        | (P.colorspace_type                    << 16)
        | ((P.num_h_slices - 1)                 << 24)
        ;
      RawFrame->Create(P.width, P.height, info);

        uint64_t Slices_BufferPos = Buffer_Size;
        while (Slices_BufferPos)
        {
            if (Slices_BufferPos < P.TailSize || Slices_Size >= Slot->Slices_Count)
            {
                Abort(); //There is a problem
                return;
            }

            size_t Size = BigEndian2int24u(Buffer + (size_t)Slices_BufferPos - P.TailSize);
            Size += P.TailSize;

            if (Size > Slices_BufferPos)
            {
                Abort(); //There is a problem
                return;
            }
            Slices_BufferPos -= Size;

            Slices[Slices_Size].BufferSize = Size;
            slice_struct& Slice_Current = Slices[Slices_Size];
            slice*& Slice_Content = Slice_Current.Content;
            if (!Slice_Content)
                Slice_Content = new slice(&P);
            Slice_Content->Init(Buffer + Slices_BufferPos, Size, keyframe, !Slices_BufferPos, RawFrame);
            Slices_Size++;
        }
        Slices_Size--;
    }
    else
    {
        Slices_Size = 0;
        slice_struct& Slice_Current = Slices[0];
        Slice_Current.BufferSize = Buffer_Size;
        slice*& Slice_Content = Slice_Current.Content;
        if (!Slice_Content)
            Slice_Content = new slice(&P);
        Slice_Content->Init(Buffer, Buffer_Size, keyframe, true, RawFrame);
    }

    Slot->Count = Slices_Size + 1;
    Slot->Started = 0;
    Slot->Finished = 0;
    if (Pool)
    {
        for (size_t i = Slices_Size; i <= Slices_Size; i--)
        {
            auto Slice = Slices[i].Content;
            Pool->submit([this, Slot, Slice] { Slice_Run(Slot, Slice); });
        }

        // Next frame when all slices of this frame are being decoded
        TRACE_SCOPE("Ours slices dispatch", RawFrame->Index);
        std::unique_lock<std::mutex> Lock(Mutex);
        Condition.wait(Lock, [&] { return Slot->Started == Slot->Count; });
    }
    else
    {
        for (size_t i = Slices_Size; i <= Slices_Size; i--)
            Slice_Run(Slot, Slices[i].Content);
    }
}

//---------------------------------------------------------------------------
void ffv1_frame::Slice_Run(ffv1_frame_slot* Slot, slice* Slice)
{
    // Slot content must not be read after the increment of Finished if this is not the last slice (the slot may be reused or deleted)
    auto Count = Slot->Count;
    if (++Slot->Started == Count)
    {
        std::lock_guard<std::mutex> Lock(Mutex);
        Condition.notify_all();
    }
    {
        TRACE_SCOPE("Slice", Slot->Frame->Index);
        Slice->Parse();
    }
    if (++Slot->Finished == Count)
        Frame_End(Slot);
}

//---------------------------------------------------------------------------
void ffv1_frame::Frame_End(ffv1_frame_slot* Slot)
{
    Slot->Frame->Finalize(P.num_h_slices, P.num_v_slices);
    if (Slot->Trace_Begin)
        trace::Span("Ours frame", Slot->Trace_Begin, trace::Now(), Slot->Frame->Index);
    auto Done = std::move(Slot->Done);
    Done(P.Error() ? true : false);

    // Notification with the lock, else this object may be deleted (after a Wait()) before the notification
    std::lock_guard<std::mutex> Lock(Mutex);
    Slot->Busy = false;
    trace::Counter("Ours frames", (int64_t)Slots_Busy());
    Condition.notify_all();
}

//---------------------------------------------------------------------------
// Mutex must be locked
size_t ffv1_frame::Slots_Busy()
{
    size_t Count = 0;
    for (size_t i = 0; i < Slots_Count; i++)
        if (Slots[i].Busy)
            Count++;
    return Count;
}

//---------------------------------------------------------------------------
void ffv1_frame::Wait()
{
    std::unique_lock<std::mutex> Lock(Mutex);
    Condition.wait(Lock, [&]
    {
        for (size_t i = 0; i < Slots_Count; i++)
            if (Slots[i].Busy)
                return false;
        return true;
    });
}

//***************************************************************************
// Errors
//***************************************************************************

//---------------------------------------------------------------------------
const char* ffv1_frame::ErrorMessage() const
{
    return P.Error();
}

//***************************************************************************
// Helpers
//***************************************************************************

//---------------------------------------------------------------------------
void ffv1_frame::Clear()
{
    Wait();
    for (size_t s = 0; s < Slots_Count; s++)
    {
        auto& Slot = Slots[s];
        if (Slot.Slices)
        {
            for (size_t i = 0; i < Slot.Slices_Count; i++)
                delete Slot.Slices[i].Content;
            delete[] Slot.Slices;
        }
        Slot.Slices = nullptr;
        Slot.Slices_Count = 0;
    }
}

