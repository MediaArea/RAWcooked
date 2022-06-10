/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
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
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
bool track_info::Init(const uint8_t* BaseData)
{
    if (RawFrame)
        return false; // Already done
    RawFrame = new raw_frame;
    RawFrame->FrameProcess = FrameWriter;

    ReversibilityData->StartParsing(BaseData);
    RawFrame->SetPre(ReversibilityData->Data(reversibility::element::BeforeData));
    RawFrame->SetPost(ReversibilityData->Data(reversibility::element::AfterData));

    auto Parser = InitOutput_Find();
    if (!Parser)
        return true;

    if (!Wrapper)
    {
        Wrapper = CreateWrapper(Format, Pool);
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
        auto Wrapper2 = (audio_wrapper*)Wrapper;
        auto Parser2 = (input_base_uncompressed_audio*)Parser;
        Wrapper2->SetConfig(Parser2->BitDepth(), Parser2->Sign(), Parser2->Endianness());
        delete Parser; // No more used
        break;
    }
    case format_kind::unknown:;
    }

    RawFrame->SetPost(buffer_or_view());

    if (ReversibilityData->Unique())
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
bool track_info::Process(const uint8_t* Data, size_t Size)
{
    if (!ReversibilityData->Unique())
    {
        RawFrame->SetPre(ReversibilityData->Data(reversibility::element::BeforeData));
        RawFrame->SetPost(ReversibilityData->Data(reversibility::element::AfterData));
        RawFrame->SetIn(ReversibilityData->Data(reversibility::element::InData));
        FrameWriter->OutputFileName = ReversibilityData->Data(reversibility::element::FileName);
        FormatPath(FrameWriter->OutputFileName);
        if (FrameWriter->OutputFileName.empty() && ReversibilityData->Count())
            Undecodable(reversibility_issue::undecodable::ReversibilityData_FrameCount);
    }
    if (Wrapper)
        Wrapper->Process(Data, Size);
    if (!ReversibilityData->Unique())
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
bool track_info::OutOfBand(const uint8_t* Data, size_t Size)
{
    // Special case : VFW
    if (Format == format::VFW)
    {
        Format = format::None;

        // Special case, real format is inside the VFW block
        if (Size <= 0x28)
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
        case 0x46465631: Format = format::FFV1; break;
        default:;
        }

        Data += 0x28;
        Size -= 0x28;
    }

    // Parse
    if (!Wrapper)
    {
        Wrapper = CreateWrapper(Format, Pool);
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
    if (!Actions[Action_Decode] && !Actions[Action_Check])
    {
        return;
    }

    // Write end of the file if the file is unique per track
    if (ReversibilityData->Unique())
    {
        RawFrame->AssignBufferView(nullptr, 0);
        RawFrame->SetPre(buffer_or_view());
        RawFrame->SetPost(ReversibilityData->Data(reversibility::element::AfterData));
        RawFrame->SetIn(buffer_or_view());
        FrameWriter->Mode[frame_writer::IsNotEnd] = false;
        RawFrame->Process();
    }

    // Checks
    if (!ReversibilityData->Unique() && Errors)
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
//
track_info::track_info(const frame_writer& FrameWriter_Source, const bitset<Action_Max>& Actions, errors* Errors, ThreadPool* Pool_) :
    input_base(Errors, Parser_ReversibilityData),
    ReversibilityData(new reversibility()),
    FrameWriter(new frame_writer(FrameWriter_Source)),
    Pool(Pool_)
{
    input_base::Actions = Actions;
}

//---------------------------------------------------------------------------
//
track_info::~track_info()
{
    delete FrameWriter;
    delete ReversibilityData;
    delete DecodedFrameParser;
    delete Wrapper;
    delete RawFrame;
}
