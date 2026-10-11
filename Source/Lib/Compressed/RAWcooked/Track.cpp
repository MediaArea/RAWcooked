/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Compressed/RAWcooked/Track.h"
#include "Lib/CoDec/Wrapper.h"
#include "Lib/Utils/FileIO/FileWriter.h"
#include "Lib/Utils/FileIO/FileChecker.h"
#include "Lib/Compressed/RAWcooked/Reversibility.h"
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include "Lib/Uncompressed/EXR/EXR.h"
#include "Lib/Uncompressed/WAV/WAV.h"
#include "Lib/Uncompressed/AIFF/AIFF.h"
#include "ThreadPool.h"
#include "Lib/Utils/MD5/MD5_Lanes.h"
#include "Lib/Utils/Trace/Trace.h"
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Frame between its decoding and the end of its output (writing or checking)
struct output_item
{
    // Frame related data, copied because reversibility data may be remapped or go to next frame before the output
    buffer                      BeforeData;
    buffer                      AfterData;
    buffer                      InData;
    string                      OutputFileName;
    uint64_t                    FileSize = 0;
    bool                        HasFrameData = false;

    // Output
    bool                        Ordered = false; // Output in order with the frame writer of the track (e.g. all frames in the same file), else in parallel with other frames
    size_t                      Order = 0;
    raw_frame*                  Frame = nullptr; // Decoded frame
    bool                        Error = false; // Decoding error
};

//---------------------------------------------------------------------------
// Outputs of decoded frames, in output threads
struct output_pending : public raw_frame_ready
{
    std::mutex                  Mutex;
    std::condition_variable     Condition;
    std::deque<output_item*>    Items; // Decoded, waiting for output
    std::vector<raw_frame*>     Frames_Free; // For reuse (memory pages)
    std::vector<std::thread>    Threads;
    size_t                      Count = 0; // Between the decoding and the end of the output
    size_t                      Order_Count = 0; // Count of frames to be output in order
    size_t                      Order_Next = 0; // Next frame to be output in order
    bool                        Order_Busy = false; // A frame to be output in order is being output
    size_t                      Writer_Generation = 0; // Frame writers of output threads are created again if the frame writer of the track is modified
    bool                        Stop = false;
    bool                        Ending = false; // No more frames, decoded frames are not reused
    size_t                      FrameSize = 0; // Size of decoded frames (same for all frames)
    size_t                      Index = 0; // Index of the next frame, for instrumentation

    // Parsing of decoded frames, in order in the parsing thread, waiting for the frame size if the file size is unknown
    struct parse_item
    {
        buffer                  BeforeData;
        buffer                  AfterData;
        uint64_t                FileSize;
    };
    std::deque<parse_item>      Parse_Items;

    void Ready(void* Opaque, bool Error)
    {
        auto Item = (output_item*)Opaque;
        Item->Error = Error;

        // Notification with the lock, else this object may be deleted (after the output of the last frame) before the notification
        std::lock_guard<std::mutex> Lock(Mutex);
        if (!FrameSize)
            FrameSize = Item->Frame->FrameSize();
        Items.push_back(Item);
        Condition.notify_all();
    }
};
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
bool track_info::Init(const uint8_t* BaseData)
{
    if (RawFrame)
        return false; // Already done
    RawFrame = new raw_frame;
    RawFrame->FrameProcess = FrameWriter;

    if (ReversibilityData)
    {
        ReversibilityData->StartParsing(BaseData);
        RawFrame->SetPre(ReversibilityData->Data(reversibility::element::BeforeData));
        RawFrame->SetPost(ReversibilityData->Data(reversibility::element::AfterData));
    }

    auto Parser = InitOutput_Find();

    if (!Wrapper)
    {
        Wrapper = CreateWrapper(Format, Pool, Actions[Action_FFmpegDecoder], Actions[Action_VulkanDecoder]);
        if (!Wrapper)
            return true;
    }
    Wrapper->RawFrame = RawFrame;
    switch (FormatKind(Format))
    {
    case format_kind::video:
    {
        auto Wrapper2 = (video_wrapper*)Wrapper;
        Wrapper2->SetWidth(Width);
        Wrapper2->SetHeight(Height);
        DecodedFrameParser = Parser; // Used for each frame
        break;
    }
    case format_kind::audio:
    {
        if (Parser)
        {
            auto Wrapper2 = (audio_wrapper*)Wrapper;
            auto Parser2 = (input_base_uncompressed_audio*)Parser;
            Wrapper2->SetConfig(Parser2->BitDepth(), Parser2->Sign(), Parser2->Endianness());
            delete Parser; // No more used
            Parser = nullptr;
        }
        break;
    }
    case format_kind::unknown:;
    }

    RawFrame->SetPost(buffer_or_view());

    if (ReversibilityData && ReversibilityData->Unique())
    {
        FrameWriter->OutputFileName = ReversibilityData->Data(reversibility::element::FileName);
        FormatPath(FrameWriter->OutputFileName);

        FrameWriter->Mode[frame_writer::IsNotEnd] = true;
        RawFrame->Process();
        FrameWriter->Mode[frame_writer::IsNotBegin] = true;
    }

    return false;
}

//---------------------------------------------------------------------------
bool track_info::Process(const uint8_t* Data, size_t Size, data_holder* Holder)
{
    if (Wrapper && Wrapper->CanSplit())
        return Process_Split(Data, Size, Holder);

    if (ReversibilityData && !ReversibilityData->Unique())
    {
        RawFrame->SetPre(ReversibilityData->Data(reversibility::element::BeforeData));
        RawFrame->SetPost(ReversibilityData->Data(reversibility::element::AfterData));
        RawFrame->SetIn(ReversibilityData->Data(reversibility::element::InData));
        FrameWriter->OutputFileName = ReversibilityData->Data(reversibility::element::FileName);
        FormatPath(FrameWriter->OutputFileName);
        if (FrameWriter->OutputFileName.empty() && ReversibilityData->Count())
            Undecodable(reversibility_issue::undecodable::ReversibilityData_FrameCount);
    }
    if (!Wrapper)
    {
        Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::Format_Undetected, string());
        return true;
    }
    if (Wrapper->Process(Data, Size, Holder))
    {
        string OutputFileName = ReversibilityData->Data(reversibility::element::FileName);
        FormatPath(OutputFileName);
        Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::Frame_Compressed_Issue, OutputFileName);
    }
    if (ReversibilityData && !ReversibilityData->Unique())
    {
        if (Actions[Action_Conch] || Actions[Action_Coherency])
        {
            RawFrame->SetPre(ReversibilityData->Data(reversibility::element::BeforeData));
            RawFrame->SetPost(ReversibilityData->Data(reversibility::element::AfterData));
            ParseDecodedFrame();
        }
        ReversibilityData->NextFrame();
    }

    return false;
}

//---------------------------------------------------------------------------
// Same as Process() but the decoding may be in other threads and the output (writing or checking) is done in output threads, during the decoding of next frames
// Outputs are in parallel if each frame is in its own file, else in order
bool track_info::Process_Split(const uint8_t* Data, size_t Size, data_holder* Holder)
{
    if (!Output_Pending)
    {
        Output_Pending = new output_pending;
        Wrapper->FrameReady = Output_Pending;
        if (Pool)
        {
            size_t Count = thread::hardware_concurrency();
            if (!Count || Count > 4)
                Count = 4; // Enough for the hash computing of several frames in parallel
            for (size_t i = 0; i < Count; i++)
                Output_Pending->Threads.emplace_back(&track_info::Output_Run, this);
        }
    }
    auto P = Output_Pending;

    // Frame related data
    auto Item = new output_item;
    Item->HasFrameData = ReversibilityData && !ReversibilityData->Unique();
    if (ReversibilityData)
    {
        Item->OutputFileName = ReversibilityData->Data(reversibility::element::FileName);
        FormatPath(Item->OutputFileName);
    }
    if (Item->HasFrameData)
    {
        Item->BeforeData = ReversibilityData->Data(reversibility::element::BeforeData);
        Item->AfterData = ReversibilityData->Data(reversibility::element::AfterData);
        Item->InData = ReversibilityData->Data(reversibility::element::InData);
        Item->FileSize = ReversibilityData->FileSize();
        if (Item->OutputFileName.empty() && ReversibilityData->Count())
            Undecodable(reversibility_issue::undecodable::ReversibilityData_FrameCount);
        ReversibilityData->NextFrame();
    }

    // Parsing of the decoded frame, in order, without the decoded content (only its size is needed)
    if (Item->HasFrameData && (Actions[Action_Conch] || Actions[Action_Coherency]))
        P->Parse_Items.push_back({ Item->BeforeData, Item->AfterData, Item->FileSize });
    Parse_Flush(false);

    // Outputs in parallel only if each frame is in its own file
    Item->Ordered = !Pool || !Item->HasFrameData || FrameWriter->Compound;

    // Frame for the decoding, with a limited count of frames between the decoding and the end of the output
    {
        std::unique_lock<std::mutex> Lock(P->Mutex);
        if (P->Count >= Wrapper->Depth())
        {
            // Pending hashes are computed now, else frames would wait for other frames which can not be decoded
            TRACE_SCOPE("Wait depth", P->Index);
            MD5_Lanes_Flush(true);
            P->Condition.wait(Lock, [&] { return P->Count < Wrapper->Depth(); });
            MD5_Lanes_Flush(false);
        }
        P->Count++;
        trace::Counter("Frames in flight", (int64_t)P->Count);
        if (Item->Ordered)
            Item->Order = P->Order_Count++;
        if (P->Frames_Free.empty())
            Item->Frame = new raw_frame;
        else
        {
            Item->Frame = P->Frames_Free.back();
            P->Frames_Free.pop_back();
        }
    }
    Item->Frame->Flavor = RawFrame->Flavor;
    Item->Frame->Index = P->Index++;
    Item->Frame->Flavor_Private = RawFrame->Flavor_Private;

    // Decoding (maybe in another thread), then output in an output thread
    Wrapper->Remaining = Item->HasFrameData ? ReversibilityData->RemainingCount() : (size_t)-1;
    Wrapper->Decode(Data, Size, Holder, Item->Frame, Item);
    if (Pool)
        return false;

    // No thread pool, output now
    {
        std::unique_lock<std::mutex> Lock(P->Mutex);
        P->Condition.wait(Lock, [&] { return !P->Items.empty(); });
        P->Items.pop_front();
    }
    Output(Item, FrameWriter);
    std::lock_guard<std::mutex> Lock(P->Mutex);
    P->Frames_Free.push_back(Item->Frame);
    delete Item;
    P->Count--;

    return false;
}

//---------------------------------------------------------------------------
// Output thread
void track_info::Output_Run()
{
    auto P = Output_Pending;
    frame_writer* Writer = nullptr; // For frames in their own file
    trace::ThreadName("Output");
    size_t Writer_Generation = 0;

    std::unique_lock<std::mutex> Lock(P->Mutex);
    for (;;)
    {
        // Next decoded frame: any frame in its own file, or next frame in order if no other frame in order is being output
        output_item* Item = nullptr;
        P->Condition.wait(Lock, [&]
        {
            for (auto It = P->Items.begin(); It != P->Items.end(); ++It)
                if (!(*It)->Ordered || (!P->Order_Busy && (*It)->Order == P->Order_Next))
                {
                    Item = *It;
                    P->Items.erase(It);
                    return true;
                }
            return P->Stop;
        });
        if (!Item)
            break; // Stop
        if (Item->Ordered)
            P->Order_Busy = true;
        else if (!Writer || Writer_Generation != P->Writer_Generation)
        {
            delete Writer;
            Writer = new frame_writer(*FrameWriter);
            Writer->Hash_Batch = true; // Hash of several frames in parallel
            Writer_Generation = P->Writer_Generation;
        }
        Lock.unlock();

        Output(Item, Item->Ordered ? FrameWriter : Writer);

        Lock.lock();
        if (Item->Ordered)
        {
            P->Order_Busy = false;
            P->Order_Next++;
        }
        raw_frame* Frame_ToDelete = nullptr;
        if (P->Ending)
            Frame_ToDelete = Item->Frame;
        else
            P->Frames_Free.push_back(Item->Frame);
        delete Item;
        P->Count--;
        trace::Counter("Frames in flight", (int64_t)P->Count);
        P->Condition.notify_all();
        if (Frame_ToDelete)
        {
            Lock.unlock();
            delete Frame_ToDelete;
            Lock.lock();
        }
    }
    Lock.unlock();
    delete Writer;
}

//---------------------------------------------------------------------------
void track_info::Output(output_item* Item, frame_writer* Writer)
{
    TRACE_SCOPE("Output", Item->Frame->Index);
    raw_frame* Frame;
    if (Item->Ordered)
    {
        // Content in the track raw frame, as without frame threading, previous content to the decoded frame for reuse
        swap(RawFrame->Planes_, Item->Frame->Planes_);
        Frame = RawFrame;
    }
    else
    {
        Frame = Item->Frame;
        Frame->FrameProcess = Writer;
    }
    if (Item->HasFrameData)
    {
        Frame->SetPre(buffer_or_view(Item->BeforeData));
        Frame->SetPost(buffer_or_view(Item->AfterData));
        Frame->SetIn(buffer_or_view(Item->InData));
        Writer->OutputFileName = Item->OutputFileName;
    }
    if (Item->Error)
        Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::Frame_Compressed_Issue, Item->OutputFileName);
    Frame->Process();
}

//---------------------------------------------------------------------------
bool track_info::Output_IsShared()
{
    return FrameWriter->Compound != nullptr;
}

//---------------------------------------------------------------------------
void track_info::Output_Wait()
{
    if (!Output_Pending)
        return;
    {
        MD5_Lanes_Flush(true); // No more frames until the end of pending outputs
        std::unique_lock<std::mutex> Lock(Output_Pending->Mutex);
        Output_Pending->Condition.wait(Lock, [this] { return !Output_Pending->Count; });
        MD5_Lanes_Flush(false);
    }
    Parse_Flush(true);
}

//---------------------------------------------------------------------------
// Parsing of decoded frames, in order, as soon as the size of decoded frames is known if the file size is unknown
void track_info::Parse_Flush(bool Force)
{
    auto P = Output_Pending;
    while (!P->Parse_Items.empty())
    {
        TRACE_SCOPE("Parse decoded frame");
        const auto& Item = P->Parse_Items.front();
        if (Item.FileSize && Item.FileSize != (uint64_t)-1)
            ParseDecodedFrame(Item.BeforeData, Item.AfterData, Item.FileSize);
        else
        {
            size_t FrameSize;
            {
                std::lock_guard<std::mutex> Lock(P->Mutex);
                FrameSize = P->FrameSize;
            }
            if (!FrameSize && !Force)
                break;
            if (DecodedFrameParser)
                DecodedFrameParser->Parse(Item.BeforeData, Item.BeforeData.Size() + FrameSize + Item.AfterData.Size());
        }
        P->Parse_Items.pop_front();
    }
}

//---------------------------------------------------------------------------
size_t track_info::Padding()
{
    return Wrapper ? Wrapper->Padding() : 0;
}

//---------------------------------------------------------------------------
bool track_info::OutOfBand(const uint8_t* Data, size_t Size)
{
    // Special case : VFW
    if (Format == ::format::VFW)
    {
        Format = ::format::None;

        // Special case, real format is inside the VFW block
        if (Size < 0x28)
            return true;

        uint32_t VFW_Size = ((uint32_t)Data[0]) | (((uint32_t)Data[1]) << 8) | (((uint32_t)Data[2]) << 16) | (((uint32_t)Data[3]) << 24);
        if (VFW_Size > Size)
            return true; // integrity issue

        int32_t CodecID = ((int32_t)Data[0x10])<<24
                        | ((int32_t)Data[0x11])<<16
                        | ((int32_t)Data[0x12])<< 8
                        | ((int32_t)Data[0x13]);
        
        switch (CodecID)
        {
        case 0x46465631: Format = ::format::FFV1; break;
        default:;
        }

        Data += 0x28;
        Size -= 0x28;
    }

    // Parse
    if (!Wrapper)
    {
        Wrapper = CreateWrapper(Format, Pool, Actions[Action_FFmpegDecoder], Actions[Action_VulkanDecoder]);
        if (!Wrapper)
            return true;
    }
    Wrapper->RawFrame = RawFrame;
    Wrapper->OutOfBand(Data, Size);

    return false;
}

//---------------------------------------------------------------------------
#define TEST_OUTPUT(_NAME, _FLAVOR) { if (auto Parser = InitOutput(new _NAME(Errors), raw_frame::flavor::_FLAVOR)) return Parser; }
input_base_uncompressed* track_info::InitOutput_Find()
{
    if (!ReversibilityData)
        return nullptr;
        
    switch (FormatKind(Format))
    {
    case format_kind::video:
        TEST_OUTPUT(dpx, DPX);
        TEST_OUTPUT(tiff, TIFF);
        TEST_OUTPUT(exr, EXR);
        break;
    case format_kind::audio:
        TEST_OUTPUT(wav, None);
        TEST_OUTPUT(aiff, None);
        break;
    case format_kind::unknown:;
    }
    return nullptr;
}

//---------------------------------------------------------------------------
input_base_uncompressed* track_info::InitOutput(input_base_uncompressed* PotentialParser, raw_frame::flavor Flavor)
{
    PotentialParser->Actions.set(Action_Encode);
    PotentialParser->Actions.set(Action_AcceptTruncated);
    if (ReversibilityData->Unique())
    {
        // The begin of the content will not be parsed again, checking now
        if (Actions[Action_Conch])
            PotentialParser->Actions.set(Action_Conch);
        if (Actions[Action_Coherency])
            PotentialParser->Actions.set(Action_Coherency);
    }
    if (ParseDecodedFrame(PotentialParser))
    {
        delete PotentialParser;
        return nullptr;
    }

    if (!PotentialParser->IsSupported())
    {
        delete PotentialParser;
        Undecodable(reversibility_issue::undecodable::ReversibilityData_UnreadableFrameHeader);
        return nullptr;
    }

    RawFrame->Flavor = Flavor;
    RawFrame->Flavor_Private = PotentialParser->Flavor;

    if (!ReversibilityData->Unique())
    {
        // The begin of the content will be checked by frame
        if (Actions[Action_Conch])
            PotentialParser->Actions.set(Action_Conch);
        if (Actions[Action_Coherency])
            PotentialParser->Actions.set(Action_Coherency);
    }

    return PotentialParser;
}

//---------------------------------------------------------------------------
void track_info::End(size_t i)
{
    Output_End();
    Output_Wait();

    if (!Actions[Action_Decode] && !Actions[Action_Check])
    {
        return;
    }

    // Write end of the file if the file is unique per track (no raw frame if there is no cluster e.g. truncated file)
    if (ReversibilityData && ReversibilityData->Unique() && RawFrame)
    {
        RawFrame->AssignBufferView(nullptr, 0);
        RawFrame->SetPre(buffer_or_view());
        RawFrame->SetPost(ReversibilityData->Data(reversibility::element::AfterData));
        RawFrame->SetIn(buffer_or_view());
        FrameWriter->Mode[frame_writer::IsNotEnd] = false;
        RawFrame->Process();
    }

    // Checks
    if (ReversibilityData && !ReversibilityData->Unique() && Errors)
    {
        if (auto ExtraCount = ReversibilityData->ExtraCount())
        {
            string OutputInfo = "Track " + to_string(i) + ", " + to_string(ExtraCount) + " frames";
            string OutputFileName = ReversibilityData->Data(reversibility::element::FileName, ReversibilityData->Count() - 1);
            FormatPath(OutputFileName);
            if (!OutputFileName.empty())
            {
                OutputInfo += " after ";
                OutputInfo += OutputFileName;
            }
            Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::Frame_Compressed_Extra, OutputInfo);
        }

        if (auto RemainingCount = ReversibilityData->RemainingCount())
        {
            string OutputFileName = ReversibilityData->Data(reversibility::element::FileName);
            FormatPath(OutputFileName);
            Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::Frame_Compressed_Missing, OutputFileName);
            if (RemainingCount > 1)
            {
                if (RemainingCount > 2)
                    Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::Frame_Compressed_Missing, "...");
                string OutputFileName2 = ReversibilityData->Data(reversibility::element::FileName, ReversibilityData->Count() - 1);
                FormatPath(OutputFileName2);
                Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::Frame_Compressed_Missing, OutputFileName2);
            }
        }
    }
}

//---------------------------------------------------------------------------
// No more frames, resources are released during the output of last frames
void track_info::Output_End()
{
    if (Output_Pending)
    {
        Wrapper->End();
        std::vector<raw_frame*> Frames;
        {
            std::lock_guard<std::mutex> Lock(Output_Pending->Mutex);
            Output_Pending->Ending = true;
            Frames.swap(Output_Pending->Frames_Free);
        }
        for (auto Frame : Frames)
            delete Frame;
    }
}

//---------------------------------------------------------------------------
void track_info::UpdateReversibility(reversibility* ContainerReversibility, input_base_uncompressed_compound* Compound, raw_frame::flavor Flavor)
{
    Output_Wait();
    FrameWriter->OutputFileName = ContainerReversibility->Data(reversibility::element::FileName);
    FormatPath(FrameWriter->OutputFileName);
    FrameWriter->Compound = Compound;
    RawFrame->Flavor = Flavor;
    if (Output_Pending)
    {
        std::lock_guard<std::mutex> Lock(Output_Pending->Mutex);
        Output_Pending->Writer_Generation++;
    }
}

//---------------------------------------------------------------------------
//
track_info::track_info(const frame_writer& FrameWriter_Source, const bitset<Action_Max>& Actions, errors* Errors, ThreadPool* Pool_) :
    input_base(Errors, Parser_ReversibilityData),
    ReversibilityData(nullptr),
    Pool(Pool_),
    FrameWriter(new frame_writer(FrameWriter_Source))
{
    input_base::Actions = Actions;
}

//---------------------------------------------------------------------------
//
track_info::~track_info()
{
    Output_Wait();
    if (Output_Pending)
    {
        {
            std::lock_guard<std::mutex> Lock(Output_Pending->Mutex);
            Output_Pending->Stop = true;
        }
        Output_Pending->Condition.notify_all();
        for (auto& Thread : Output_Pending->Threads)
            Thread.join();
        for (auto Frame : Output_Pending->Frames_Free)
            delete Frame;
        delete Output_Pending;
    }
    delete FrameWriter;
    delete ReversibilityData;
    delete DecodedFrameParser;
    delete Wrapper;
    delete RawFrame;
}
