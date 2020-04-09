/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Compressed/Matroska/Matroska.h"
#include "Lib/Utils/FileIO/FileWriter.h"
#include "Lib/Utils/FileIO/FileChecker.h"
#include "Lib/Compressed/RAWcooked/Reversibility.h"
#include "Lib/Compressed/RAWcooked/Track.h"
#include "Lib/Uncompressed/HashSum/HashSum.h"
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreorder"
#endif
#include "ThreadPool.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#include "zlib.h"
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace matroska_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "file smaller than expected",
};

enum code : uint8_t
{
    BufferOverflow,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unparsable

const char** ErrorTexts[] =
{
    undecodable::MessageText,
    nullptr,
    nullptr,
    nullptr,
};

static_assert(error::type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // matroska_issue

using namespace matroska_issue;

//---------------------------------------------------------------------------
// Matroska parser

#define ELEMENT_BEGIN(_VALUE) \
matroska::call matroska::SubElements_##_VALUE(uint64_t Name) \
{ \
    switch (Name) \
    { \

#define ELEMENT_CASE(_VALUE,_NAME) \
    case 0x##_VALUE:  Levels[Level].SubElements = &matroska::SubElements_##_NAME;  return &matroska::_NAME;

#define ELEMENT_VOID(_VALUE,_NAME) \
    case 0x##_VALUE:  Levels[Level].SubElements = &matroska::SubElements_Void;  return &matroska::_NAME;


#define ELEMENT_END() \
    default:                        return SubElements_Void(Name); \
    } \
} \

ELEMENT_BEGIN(_)
ELEMENT_CASE( 8538067, Segment)
ELEMENT_END()

ELEMENT_BEGIN(Segment)
ELEMENT_CASE( 941A469, Segment_Attachments)
ELEMENT_CASE( F43B675, Segment_Cluster)
ELEMENT_CASE( 654AE6B, Segment_Tracks)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments)
ELEMENT_CASE(    21A7, Segment_Attachments_AttachedFile)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile)
ELEMENT_VOID(     66E, Segment_Attachments_AttachedFile_FileName)
ELEMENT_CASE(     65C, Segment_Attachments_AttachedFile_FileData)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData)
ELEMENT_CASE(    7261, Segment_Attachments_AttachedFile_FileData_RawCookedAttachment)
ELEMENT_CASE(    7273, Segment_Attachments_AttachedFile_FileData_RawCookedSegment)
ELEMENT_CASE(    7274, Segment_Attachments_AttachedFile_FileData_RawCookedTrack)
ELEMENT_CASE(    7262, Segment_Attachments_AttachedFile_FileData_RawCookedBlock)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData_RawCookedAttachment)
ELEMENT_VOID(      20, Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileHash)
ELEMENT_VOID(      10, Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileName)
ELEMENT_VOID(      30, Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileSize)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData_RawCookedBlock)
ELEMENT_VOID(       2, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_AfterData)
ELEMENT_VOID(       1, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_BeforeData)
ELEMENT_VOID(       5, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_InData)
ELEMENT_VOID(      20, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileHash)
ELEMENT_VOID(      10, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileName)
ELEMENT_VOID(      30, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileSize)
ELEMENT_VOID(       4, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionAfterData)
ELEMENT_VOID(       3, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionBeforeData)
ELEMENT_VOID(      11, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionFileName)
ELEMENT_VOID(       6, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionInData)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData_RawCookedSegment)
ELEMENT_VOID(      70, Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryName)
ELEMENT_VOID(      71, Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryVersion)
ELEMENT_VOID(      72, Segment_Attachments_AttachedFile_FileData_RawCookedSegment_PathSeparator)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData_RawCookedTrack)
ELEMENT_VOID(       2, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_AfterData)
ELEMENT_VOID(       1, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_BeforeData)
ELEMENT_VOID(       5, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_InData)
ELEMENT_VOID(      20, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_FileHash)
ELEMENT_VOID(      10, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_FileName)
ELEMENT_VOID(      70, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryName)
ELEMENT_VOID(      71, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryVersion)
ELEMENT_VOID(       4, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseAfterData)
ELEMENT_VOID(       3, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseBeforeData)
ELEMENT_VOID(      11, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseFileName)
ELEMENT_VOID(       6, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseInData)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Cluster)
ELEMENT_VOID(      23, Segment_Cluster_SimpleBlock)
ELEMENT_VOID(      67, Segment_Cluster_Timestamp)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Tracks)
ELEMENT_CASE(      2E, Segment_Tracks_TrackEntry)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Tracks_TrackEntry)
ELEMENT_VOID(       6, Segment_Tracks_TrackEntry_CodecID)
ELEMENT_VOID(    23A2, Segment_Tracks_TrackEntry_CodecPrivate)
ELEMENT_CASE(      60, Segment_Tracks_TrackEntry_Video)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Tracks_TrackEntry_Video)
ELEMENT_VOID(      30, Segment_Tracks_TrackEntry_Video_PixelWidth)
ELEMENT_VOID(      3A, Segment_Tracks_TrackEntry_Video_PixelHeight)
ELEMENT_END()

//---------------------------------------------------------------------------
// Glue
void matroska_ProgressIndicator_Show(matroska* M)
{
    M->ProgressIndicator_Show();
}

//---------------------------------------------------------------------------
matroska::matroska(const string& OutputDirectoryName, user_mode* Mode, ask_callback Ask_Callback, errors* Errors_Source) :
    input_base(Errors_Source, Parser_Matroska),
    Hashes_FromRAWcooked(new hashes(Errors_Source)),
    Hashes_FromAttachments(new hashes(Errors_Source)),
    FrameWriter_Template(new frame_writer(OutputDirectoryName, Mode, Ask_Callback, this, Errors_Source))
{
}

//---------------------------------------------------------------------------
matroska::~matroska()
{
    Shutdown();
    delete Hashes_FromRAWcooked;
    delete Hashes_FromAttachments;
    delete FrameWriter_Template;
}

//---------------------------------------------------------------------------
void matroska::Shutdown()
{
    // Tracks
    for (size_t i = 0; i < TrackInfo.size(); i++)
    {
        track_info* TrackInfo_Current = TrackInfo[i];
        TrackInfo_Current->End(i);
        delete TrackInfo_Current;
    }
    TrackInfo.clear();

    // Hashes
    if (Hashes_FromRAWcooked)
    {
        if (ReversibilityCompat >= Compat_18_10_1)
            Hashes_FromRAWcooked->RemoveEmptyFiles();
        Hashes_FromRAWcooked->Finish();
    }
    if (Hashes_FromAttachments)
    {
        Hashes_FromAttachments->RemoveEmptyFiles(); // Attachments don't have files with a size of 0
        Hashes_FromAttachments->Finish();
    }

    // Threads
    if (FramesPool)
    {
        FramesPool->shutdown();
        delete FramesPool;
        FramesPool = nullptr;
    }

    // Check
    if (Errors)
    {
        bool ReversibilityFileHasNames = false;
        for (const auto& AttachedFile : AttachedFiles)
        {
            if (AttachedFile.second.Flags[ReversibilityFileHasName])
            {
                if (ReversibilityCompat >= Compat_18_10_1 && !AttachedFile_FileNames_IsHash.empty()) // In previous versions hash files were not listed in reversibility file
                {
                    for (const auto& Name : AttachedFile_FileNames_IsHash)
                        AttachedFiles[Name].Flags.set(ReversibilityFileHasName); // Fake
                    AttachedFile_FileNames_IsHash.clear();
                }

                ReversibilityFileHasNames = true;
                break;
            }
        }

        for (const auto& AttachedFile : AttachedFiles)
        {
            if (AttachedFile.second.Flags[ReversibilityFileHasName])
            {
                if (AttachedFile.second.Flags[IsInFromAttachments])
                {
                    if (AttachedFile.second.Flags[ReversibilityFileHasSize] && AttachedFile.second.FileSizeFromReversibilityFile != AttachedFile.second.FileSizeFromAttachments)
                        Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)(filechecker_issue::undecodable::FileComparison), AttachedFile.first);
                }
                else
                {
                    if (AttachedFile.second.Flags[ReversibilityFileHasSize] && AttachedFile.second.FileSizeFromReversibilityFile) // If no size info it is impossible to know if attachment should be present (size of 0) or not (size not of 0)
                        Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)(filechecker_issue::undecodable::Attachment_Compressed_Missing), AttachedFile.first);
                }
            }
            else
            {
                if (AttachedFile.second.Flags[IsInFromAttachments])
                {
                    if (ReversibilityFileHasNames)
                        Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)(filechecker_issue::undecodable::Attachment_Compressed_Extra), AttachedFile.first);
                }
                else
                {
                    // Should never happen
                    Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)(filechecker_issue::undecodable::Attachment_Compressed_Missing), AttachedFile.first);
                    Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)(filechecker_issue::undecodable::Attachment_Compressed_Extra), AttachedFile.first);
                }
            }
        }
    }
}

//---------------------------------------------------------------------------
matroska::call matroska::SubElements_Void(uint64_t /*Name*/)
{
    Levels[Level].SubElements = &matroska::SubElements_Void; return &matroska::Void;
}

//---------------------------------------------------------------------------
void matroska::ParseBuffer()
{
    if (Buffer.Size() < 4 || Buffer[0] != 0x1A || Buffer[1] != 0x45 || Buffer[2] != 0xDF || Buffer[3] != 0xA3)
        return;
                                                                                                    
    Buffer_Offset = 0;
    Level = 0;

    // Progress indicator
    Cluster_Timestamp = 0;
    Block_Timestamp = 0;
    thread* ProgressIndicator_Thread;
    if (!Quiet)
        ProgressIndicator_Thread=new thread(matroska_ProgressIndicator_Show, this);
    
    // Config
    if (NoWrite)
        FrameWriter_Template->Mode.set(frame_writer::NoWrite);
    if (NoOutputCheck)
        FrameWriter_Template->Mode.set(frame_writer::NoOutputCheck);

    Levels[Level].Offset_End = Buffer.Size();
    Levels[Level].SubElements = &matroska::SubElements__;
    Level++;

    size_t Buffer_Offset_LowerLimit = 0; // Used for indicating the system that we'll not need anymore memory below this value 

    while (Buffer_Offset < Buffer.Size())
    {
        uint64_t Name = Get_EB();
        uint64_t Size = Get_EB();
        if (Size <= Levels[Level - 1].Offset_End - Buffer_Offset)
            Levels[Level].Offset_End = Buffer_Offset + Size;
        else
            Levels[Level].Offset_End = Levels[Level - 1].Offset_End;
        call Call = (this->*Levels[Level - 1].SubElements)(Name);
        IsList = false;
        (this->*Call)();
        if (!IsList)
            Buffer_Offset = Levels[Level].Offset_End;
        if (Buffer_Offset < Levels[Level].Offset_End)
            Level++;
        else
        {
            while (Level && Buffer_Offset >= Levels[Level - 1].Offset_End)
            {
                Levels[Level].SubElements = nullptr;
                Level--;
            }
        }

        // Check if we can indicate the system that we'll not need anymore memory below this value, without indicating it too much
        if (Buffer_Offset > Buffer_Offset_LowerLimit + 1024 * 1024) // TODO: when multi-threaded frame decoding is implemented, we need to check that all thread don't need anymore memory below this value 
        {
            FileMap->Remap();
            Buffer = *FileMap;
            Buffer_Offset_LowerLimit = Buffer_Offset;
        }
    }

    // Progress indicator
    Buffer_Offset = Buffer.Size();
    if (!Quiet)
    {
        ProgressIndicator_IsEnd.notify_one();
        ProgressIndicator_Thread->join();
        delete ProgressIndicator_Thread;
    }
}

//---------------------------------------------------------------------------
void matroska::BufferOverflow()
{
    Undecodable(undecodable::BufferOverflow);
}

//---------------------------------------------------------------------------
void matroska::Void()
{
}

//---------------------------------------------------------------------------
void matroska::Segment()
{
    SetDetected();
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile()
{
    IsList = true;

    AttachedFile_FileName.clear();
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileName()
{
    AttachedFile_FileName.assign((const char*)Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    FormatPath(AttachedFile_FileName);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData()
{
    bool IsAlpha2, IsEBML;
    if (Levels[Level].Offset_End - Buffer_Offset < 3 || Buffer[Buffer_Offset + 0] != 0x20 || Buffer[Buffer_Offset + 1] != 0x72 || (Buffer[Buffer_Offset + 2] != 0x62 && Buffer[Buffer_Offset + 2] != 0x74))
        IsAlpha2 = false;
    else
        IsAlpha2 = true;
    if (Levels[Level].Offset_End - Buffer_Offset < 4 || Buffer[Buffer_Offset + 0] != 0x1A || Buffer[Buffer_Offset + 1] != 0x45 || Buffer[Buffer_Offset + 2] != 0xDF || Buffer[Buffer_Offset + 3] != 0xA3)
        IsEBML = false;
    else
        IsEBML = true;
    if (IsAlpha2 || IsEBML)
    {
        // This is a RAWcooked file, not intended to be demuxed
        IsList = true;
        TrackInfo_Pos = (size_t)-1;

        return;
    }

    // Test if it is hash file
    //if (!Hashes) // If hashes are provided from elsewhere, they were already tests, not doing the test twice. TODO: test is removed because we need to know if hte file is an hash for older versions, in the future it should be set back for performance
    {
        hashsum HashSum;
        HashSum.HomePath = AttachedFile_FileName;
        HashSum.List = Hashes_FromAttachments;
        HashSum.Parse(buffer_view(Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset));
        if (HashSum.IsDetected())
        {
            if (Hashes_FromAttachments)
                Hashes_FromAttachments->Ignore(AttachedFile_FileName);
            AttachedFile_FileNames_IsHash.insert(AttachedFile_FileName);
        }
    }

    // Output file
    {
        raw_frame RawFrame;
        RawFrame.SetPre(buffer_view(Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset));

        frame_writer FrameWriter(FrameWriter_Template);
        FrameWriter.OutputFileName = AttachedFile_FileName;
        RawFrame.FrameProcess = &FrameWriter;
        RawFrame.Process();
    }

    auto& AttachedFile = AttachedFiles[AttachedFile_FileName];
    AttachedFile.FileSizeFromAttachments = Levels[Level].Offset_End - Buffer_Offset;
    AttachedFile.Flags.set(IsInFromAttachments);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedAttachment()
{
    IsList = true;
    AttachedFile_FileName.clear();
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileHash()
{
    if (AttachedFile_FileName.empty())
        return; // File name should come first. TODO: support when file name comes after
    if (Levels[Level].Offset_End - Buffer_Offset != 17 || Buffer[Buffer_Offset] != 0x00)
        return; // MD5 support only
    Buffer_Offset++;

    array<uint8_t, 16> Hash;
    memcpy(Hash.data(), Buffer.Data() + Buffer_Offset, Hash.size());
    Hashes_FromRAWcooked->FromHashFile(AttachedFile_FileName, Hash);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileName()
{
    buffer Output;
    Uncompress(Output); // TODO: avoid new/delete
    AttachedFile_FileName.assign((char*)Output.Data(), Output.Size());
    if (AttachedFile_FileName.empty())
        return; // Not valid, ignoring

    auto& AttachedFile = AttachedFiles[AttachedFile_FileName];
    AttachedFile.Flags.set(ReversibilityFileHasName);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileSize()
{
    if (AttachedFile_FileName.empty())
        return; // File name should come first. TODO: support when file name comes after
    uint64_t Size = Levels[Level].Offset_End - Buffer_Offset;
    if (Size > 8)
        return; // Not supported

    uint64_t FileSize = 0;
    for (; Size; Size--)
    {
        FileSize <<= 8;
        FileSize |= Buffer[Buffer_Offset++];
    }
    auto& AttachedFile = AttachedFiles[AttachedFile_FileName];
    AttachedFile.FileSizeFromReversibilityFile = FileSize;
    AttachedFile.Flags.set(ReversibilityFileHasSize);

    // File with a size of 0 are not in attachment, it is created here
    if (!FileSize)
    {
        raw_frame RawFrame;

        frame_writer FrameWriter(FrameWriter_Template);
        FrameWriter.OutputFileName = AttachedFile_FileName;
        RawFrame.FrameProcess = &FrameWriter;
        RawFrame.Process();
    }
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock()
{
    IsList = true;

    TrackInfo[TrackInfo_Pos]->ReversibilityData->NewFrame();
    RAWcooked_FileNameIsValid = false;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileHash()
{
    if (!RAWcooked_FileNameIsValid)
        return; // File name should come first. TODO: support when file name comes after
    if (Levels[Level].Offset_End - Buffer_Offset != 17 || Buffer[Buffer_Offset] != 0x00)
        return; // MD5 support only
    Buffer_Offset++;

    track_info* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    array<uint8_t, 16> Hash;
    memcpy(Hash.data(), Buffer.Data() + Buffer_Offset, Hash.size());
    Hashes_FromRAWcooked->FromHashFile(TrackInfo_Current->ReversibilityData->Data(reversibility::element::FileName, TrackInfo_Current->ReversibilityData->Count()), Hash);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileSize()
{
    uint64_t Value = 0;
    for (uint64_t Size = Levels[Level].Offset_End - Buffer_Offset; Size; Size--)
    {
        Value <<= 8;
        Value |= Buffer[Buffer_Offset++];
    }

    TrackInfo[TrackInfo_Pos]->ReversibilityData->SetFileSize(Value);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedSegment()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryName()
{
    RAWcooked_LibraryName = string((const char*)Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    RejectIncompatibleVersions();
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryVersion()
{
    RAWcooked_LibraryVersion = string((const char*)Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    RejectIncompatibleVersions();
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedSegment_PathSeparator()
{
    string RAWcooked_PathSeparator = string((const char*)Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    if (RAWcooked_PathSeparator != "/")
    {
        std::cerr << "Path separator not / is not supported, exiting" << std::endl;
        exit(1);
    }
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack()
{
    IsList = true;

    if (!FramesPool)
    {
        FramesPool = new ThreadPool(1);
        FramesPool->init();
    }

    TrackInfo_Pos++;
    if (TrackInfo_Pos >= TrackInfo.size())
        TrackInfo.push_back(new track_info(FrameWriter_Template, Actions, Errors));
}


//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_FileHash()
{
    if (!RAWcooked_FileNameIsValid)
        return; // File name should come first. TODO: support when file name comes after
    if (Levels[Level].Offset_End - Buffer_Offset != 17 || Buffer[Buffer_Offset] != 0x00)
        return; // MD5 support only
    Buffer_Offset++;

    track_info* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    array<uint8_t, 16> Hash;
    memcpy(Hash.data(), Buffer.Data() + Buffer_Offset, Hash.size());
    Hashes_FromRAWcooked->FromHashFile(TrackInfo_Current->ReversibilityData->Data(reversibility::element::FileName), Hash);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryName()
{
    // Note: LibraryName in RawCookedTrack is out of spec (alpha 1&2)
    RAWcooked_LibraryName = string((const char*)Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    RejectIncompatibleVersions();
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryVersion()
{
    // Note: LibraryVersion in RawCookedTrack is out of spec (alpha 1&2)
    RAWcooked_LibraryVersion = string((const char*)Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    RejectIncompatibleVersions();
}

//---------------------------------------------------------------------------
void matroska::Segment_Cluster()
{
    IsList = true;

    // Check if Hashes check is useful
    if (Hashes_FromRAWcooked)
    {
        Hashes_FromRAWcooked->WouldBeError = true;
        if (!Hashes_FromRAWcooked->NoMoreHashFiles())
        {
            delete Hashes_FromRAWcooked;
            Hashes_FromRAWcooked = nullptr;
        }
    }
    if (Hashes_FromAttachments)
    {
        if (!Hashes_FromAttachments->NoMoreHashFiles())
        {
            delete Hashes_FromAttachments;
            Hashes_FromAttachments = nullptr;
        }
    }

    // Init
    for (const auto& TrackInfo_Current : TrackInfo)
        if (TrackInfo_Current && TrackInfo_Current->Init())
        {
            //TODO handle errors
        }
}

//---------------------------------------------------------------------------
void matroska::Segment_Cluster_SimpleBlock()
{
    if (Levels[Level].Offset_End - Buffer_Offset > 4)
    {
        uint8_t TrackID = Buffer[Buffer_Offset] & 0x7F;
        if (!TrackID || TrackID > TrackInfo.size())
            return; // Problem
        TrackInfo_Pos = TrackID - 1;
        track_info* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

        // Timestamp
        Block_Timestamp = ((Buffer[Buffer_Offset + 1] << 8) | Buffer[Buffer_Offset + 2]);

        // Parsing
        auto CurrentBufferOffset = Buffer_Offset + 4;
        auto CurrentBufferData = Buffer.Data() + CurrentBufferOffset;
        auto CurrentBufferSize = Levels[Level].Offset_End - CurrentBufferOffset;
        TrackInfo_Current->Process(CurrentBufferData, CurrentBufferSize);
    }
}

//---------------------------------------------------------------------------
void matroska::Segment_Cluster_Timestamp()
{
    Cluster_Timestamp = 0;
    while (Buffer_Offset < Levels[Level].Offset_End)
    {
        Cluster_Timestamp <<= 8;
        Cluster_Timestamp |= Buffer[Buffer_Offset];
        Buffer_Offset++;
    }
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks()
{
    IsList = true;

    TrackInfo_Pos = (size_t)-1;
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry()
{
    IsList = true;

    TrackInfo_Pos++;
    if (TrackInfo_Pos >= TrackInfo.size())
        TrackInfo.push_back(new track_info(FrameWriter_Template, Actions, Errors));
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_Video()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_CodecID()
{
    track_info* TrackInfo_Current = TrackInfo[TrackInfo_Pos];
    TrackInfo_Current->SetFormat(string((const char*)Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset).c_str());
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_CodecPrivate()
{
    track_info* TrackInfo_Current = TrackInfo[TrackInfo_Pos];
    TrackInfo_Current->OutOfBand(Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_Video_PixelWidth()
{
    uint32_t Data = 0;
    if (Levels[Level].Offset_End - Buffer_Offset == 1)
        Data = ((uint32_t)Buffer[Buffer_Offset]);
    if (Levels[Level].Offset_End - Buffer_Offset == 2)
        Data = (((uint32_t)Buffer[Buffer_Offset]) << 8) | ((uint32_t)Buffer[Buffer_Offset + 1]);

    track_info* TrackInfo_Current = TrackInfo[TrackInfo_Pos];
    TrackInfo_Current->SetWidth(Data);
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_Video_PixelHeight()
{
    uint32_t Data = 0;
    if (Levels[Level].Offset_End - Buffer_Offset == 1)
        Data = ((uint32_t)Buffer[Buffer_Offset]);
    if (Levels[Level].Offset_End - Buffer_Offset == 2)
        Data = (((uint32_t)Buffer[Buffer_Offset]) << 8) | ((uint32_t)Buffer[Buffer_Offset + 1]);

    track_info* TrackInfo_Current = TrackInfo[TrackInfo_Pos];
    TrackInfo_Current->SetHeight(Data);
}

//***************************************************************************
// Utils
//***************************************************************************

static void ShowByteRate(float ByteRate)
{
    if (ByteRate < 1024)
        cerr << ByteRate << " B/s";
    else if (ByteRate < 1024 * 1024)
        cerr << setprecision(ByteRate > 10 * 1024 ? 1 : 0) << ByteRate / 1024 << " KiB/s";
    else if (ByteRate < 1024 * 1024 * 1024)
        cerr << setprecision(ByteRate < 10LL * 1024 * 1024 ? 1 : 0) << ByteRate / 1024 / 1024 << " MiB/s";
    else
        cerr << setprecision(ByteRate < 10LL * 1024 * 1024 * 1024 ? 1 : 0) << ByteRate / 1024 / 1024 / 1024 << " GiB/s";
}
static void ShowRealTime(float RealTime)
{
    cerr << setprecision(2) << RealTime << "x realtime";
}

//---------------------------------------------------------------------------
void matroska::ProgressIndicator_Show()
{
    // Configure progress indicator precision
    size_t ProgressIndicator_Value = (size_t)-1;
    size_t ProgressIndicator_Frequency = 100;
    streamsize Precision = 0;
    cerr.setf(ios::fixed, ios::floatfield);
    
    // Configure benches
    using namespace chrono;
    steady_clock::time_point Clock_Init = steady_clock::now();
    steady_clock::time_point Clock_Previous = Clock_Init;
    uint64_t Buffer_Offset_Previous = 0;
    uint64_t Timestamp_Previous = 0;

    // Show progress indicator at a specific frequency
    const chrono::seconds Frequency = chrono::seconds(1);
    size_t StallDetection = 0;
    mutex Mutex;
    unique_lock<mutex> Lock(Mutex);
    do
    {
        if (ProgressIndicator_IsPaused)
            continue;

        size_t ProgressIndicator_New = (size_t)(((float)Buffer_Offset) * ProgressIndicator_Frequency / Buffer.Size());
        if (ProgressIndicator_New == ProgressIndicator_Value)
        {
            StallDetection++;
            if (StallDetection >= 4)
            {
                while (ProgressIndicator_New == ProgressIndicator_Value && ProgressIndicator_Frequency < 10000)
                {
                    ProgressIndicator_Frequency *= 10;
                    ProgressIndicator_Value *= 10;
                    Precision++;
                    ProgressIndicator_New = (size_t)(((float)Buffer_Offset) * ProgressIndicator_Frequency / Buffer.Size());
                }
            }
        }
        else
            StallDetection = 0;
        if (ProgressIndicator_New != ProgressIndicator_Value)
        {
            uint64_t Timestamp = (Cluster_Timestamp + Block_Timestamp);
            float ByteRate = 0, RealTime = 0;
            if (ProgressIndicator_Value != (size_t)-1)
            {
                steady_clock::time_point Clock_Current = steady_clock::now();
                steady_clock::duration Duration = Clock_Current - Clock_Previous;
                ByteRate = (float)(Buffer_Offset - Buffer_Offset_Previous) * 1000 / duration_cast<milliseconds>(Duration).count();
                RealTime = (float)(Timestamp - Timestamp_Previous) / duration_cast<milliseconds>(Duration).count();
                Clock_Previous = Clock_Current;
                Buffer_Offset_Previous = Buffer_Offset;
                Timestamp_Previous = Timestamp;
            }
            Timestamp /= 1000;
            cerr << '\r';
            cerr << "Time=" << (Timestamp / 36000) % 6 << (Timestamp / 3600) % 10 << ':' << (Timestamp / 600) % 6 << (Timestamp / 60) % 10 << ':' << (Timestamp / 10) % 6 << Timestamp % 10;
            cerr << " (" << setprecision(Precision) << ((float)ProgressIndicator_New) * 100 / ProgressIndicator_Frequency << "%)";
            if (ByteRate)
            {
                cerr << ", ";
                ShowByteRate(ByteRate);
            }
            if (RealTime)
            {
                cerr << ", ";
                ShowRealTime(RealTime);
            }
            cerr << "    "; // Clean up in case there is less content outputted than the previous time

            ProgressIndicator_Value = ProgressIndicator_New;
        }
    }
    while (ProgressIndicator_IsEnd.wait_for(Lock, Frequency) == cv_status::timeout && Buffer_Offset != Buffer.Size());

    // Show summary
    steady_clock::time_point Clock_Current = steady_clock::now();
    steady_clock::duration Duration = Clock_Current - Clock_Init;
    float ByteRate = (float)(Buffer.Size()) * 1000 / duration_cast<milliseconds>(Duration).count();
    uint64_t Timestamp = (Cluster_Timestamp + Block_Timestamp);
    float RealTime = (float)(Timestamp) / duration_cast<milliseconds>(Duration).count();
    cerr << '\r';
    if (ByteRate)
    {
        ShowByteRate(ByteRate);
    }
    if (ByteRate && RealTime)
        cerr << ", ";
    if (RealTime)
    {
        ShowRealTime(RealTime);
    }
    cerr << "                              \n"; // Clean up in case there is less content outputted than the previous time
}

//---------------------------------------------------------------------------
bool track_info::ParseDecodedFrame(input_base_uncompressed* Parser)
{
    bool ParseResult;
    auto FrameParser = Parser ? Parser : DecodedFrameParser;
    if (!FrameParser)
        return true;
    auto FileSize = ReversibilityData->GetFileSize();
    if (FileSize && FileSize != (uint64_t)-1)
    {
        auto Pre_Size = RawFrame->Pre().Size();
        auto Post_Size = RawFrame->Post().Size();
        auto Post_Offset = FileSize - Post_Size;
        if (Pre_Size >= FileSize || Post_Size >= FileSize || Pre_Size >= Post_Offset)
            return true; // Overlaps detected
        buffer Frame_Buffer;
        Frame_Buffer.Create(FileSize); // TODO: more optimal method without allocation of the full file size and without new/delete
        Frame_Buffer.CopyLimit(RawFrame->Pre());
        Frame_Buffer.SetZero(Pre_Size, Post_Offset - Pre_Size);
        Frame_Buffer.CopyLimit(Post_Offset, RawFrame->Post());
        ParseResult = FrameParser->Parse(Frame_Buffer);
    }
    else
    {
        ParseResult = FrameParser->Parse(RawFrame->Pre(), Parser ? (decltype(RawFrame->TotalSize()))-1 : RawFrame->TotalSize());
    }
    return ParseResult;
}

//---------------------------------------------------------------------------
void matroska::Uncompress(buffer& Output)
{
    uint64_t RealBuffer_Size = Get_EB();
    if (RealBuffer_Size)
    {
        Output.Create(RealBuffer_Size);

        uLongf t = (uLongf)RealBuffer_Size;
        if (uncompress((Bytef*)Output.Data(), &t, (const Bytef*)Buffer.Data() + Buffer_Offset, (uLong)(Levels[Level].Offset_End - Buffer_Offset))<0)
        {
            Output.Clear();
        }
        if (t != RealBuffer_Size)
        {
            Output.Clear();
        }
    }
    else
    {
        StoreFromCurrentToEndOfElement(Output);
    }
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedxxx_yyy(reversibility::element Element, type Type)
{
    buffer Buffer;
    Uncompress(Buffer);

    auto& ReversibilityData = TrackInfo[TrackInfo_Pos]->ReversibilityData;

    switch (Element)
    {
        case reversibility::element::FileName:
            RAWcooked_FileNameIsValid = true;
            break;
        default:;
    }

    switch (Type)
    {
        case type::Track_MaskBase:
            ReversibilityData->SetDataMask(Element, move(Buffer));
            return;
        case type::Track_:
            ReversibilityData->SetUnique();
            break;
        default:;
    }

    ReversibilityData->SetData(Element, move(Buffer), Type == type::Block_MaskAddition);
}

//---------------------------------------------------------------------------
void matroska::StoreFromCurrentToEndOfElement(buffer &Output)
{
    Output.CopyExpand(Buffer.Data() + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
}

//---------------------------------------------------------------------------
void matroska::RejectIncompatibleVersions()
{
    if ((RAWcooked_LibraryName == "__RAWCooked__" || RAWcooked_LibraryName == "__RAWcooked__")  && RAWcooked_LibraryVersion == "__NOT FOR PRODUCTION Alpha 1__") // RAWcooked Alpha 1 is not supported
    {
        std::cerr << RAWcooked_LibraryName << "version " << RAWcooked_LibraryVersion << " is not supported, exiting" << std::endl;
        exit(1);
    }

    if (RAWcooked_LibraryName == "RAWcooked" && !RAWcooked_LibraryVersion.empty())
    {
        if (RAWcooked_LibraryVersion < "18.10.1.20200219")
            ReversibilityCompat = Compat_18_10_1;
    }
}
