/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Matroska/Matroska_Common.h"
#include "Lib/DPX/DPX.h"
#include "Lib/TIFF/TIFF.h"
#include "Lib/WAV/WAV.h"
#include "Lib/AIFF/AIFF.h"
#include "Lib/HashSum/HashSum.h"
#include "Lib/RawFrame/RawFrame.h"
#include "Lib/Config.h"
#include "Lib/FileIO.h"
#include "Reed-Solomon/Reed-Solomon.h"
#include <cstdlib>
#include <algorithm>
#include <cstdio>
#include <thread>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "ThreadPool.h"
#include "FLAC/stream_decoder.h"
#include "zlib.h"
extern "C"
{
#include "md5.h"
}
#if defined(_WIN32) || defined(_WINDOWS)
    #include <io.h> // File existence
    #include <direct.h> // Directory creation
    #define access _access_s
    #define mkdir _mkdir
#else
    #include <sys/stat.h>
    #include <dirent.h>
    #include <unistd.h>
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace filewriter_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "cannot create directory",
    "cannot create file",
    "file already exists",
    "cannot write in the file",
    "cannot seek in the file",
};

enum code : uint8_t
{
    CreateDirectory,
    FileCreate,
    FileAlreadyExists,
    FileWrite,
    FileSeek,
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

static_assert(error::Type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // filewriter_issue

namespace filechecker_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "cannot open file for reading",
    "files are not same",
    "input file, it is corrupted, can not correct so many errors",
};

enum code : uint8_t
{
    FileOpen,
    FileComparison,
    Erasure_TooManyErrors,
    Max
};

static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage);

} // undecodable

namespace invalid
{

static const char* MessageText[] =
{
    "input file, it is corrupted, use --fix for fixing it",
    "input file, it is corrupted (parity part), use --fix for rebuilding",
};

enum code : uint8_t
{
    Erasure_DataCorrupted,
    Erasure_ParityCorrupted,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // invalid

const char** ErrorTexts[] =
{
    undecodable::MessageText,
    nullptr,
    nullptr,
    invalid::MessageText,
};

static_assert(error::Type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // filechecker_issue

//---------------------------------------------------------------------------
frame_writer::~frame_writer()
{
    delete (MD5_CTX*)MD5;
}

//---------------------------------------------------------------------------
void frame_writer::FrameCall(raw_frame* RawFrame, const string& OutputFileName)
{
    // Post-processing
    if (RawFrame->Buffer && RawFrame->In && RawFrame->Buffer_Size == RawFrame->In_Size)
        for (size_t i = 0; i < RawFrame->In_Size; i++)
            RawFrame->Buffer[i] ^= RawFrame->In[i];
    if (RawFrame->Planes.size() == 1 && RawFrame->Planes[0] && RawFrame->Planes[0]->Buffer && RawFrame->In && RawFrame->Planes[0]->Buffer_Size == RawFrame->In_Size)
        for (size_t i = 0; i < RawFrame->In_Size; i++)
            RawFrame->Planes[0]->Buffer[i] ^= RawFrame->In[i];

    if (!Mode[IsNotBegin])
    {
        if (!Mode[NoWrite])
        {
            // Open output file in writing mode only if the file does not exist
            file::return_value Result = File_Write.Open_WriteMode(BaseDirectory, OutputFileName, true);
            switch (Result)
            {
                case file::OK:
                case file::Error_FileAlreadyExists:
                    break;
                default:
                    if (Errors)
                        switch (Result)
                        {
                            case file::Error_CreateDirectory:       Errors->Error(IO_FileWriter, error::Undecodable, (error::generic::code)filewriter_issue::undecodable::CreateDirectory, OutputFileName); break;
                            case file::Error_FileCreate:            Errors->Error(IO_FileWriter, error::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileCreate, OutputFileName); break;
                            default:                                Errors->Error(IO_FileWriter, error::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName); break;
                        }
                    return;
            }
        }

        if (!Mode[NoOutputCheck] && !File_Write.IsOpen())
        {
            // File already exists, we want to check it
            if (File_Read.Open(BaseDirectory + OutputFileName))
            {
                Offset = (size_t)-1;
                SizeOnDisk = (size_t)-1;
                if (Errors)
                    Errors->Error(IO_FileChecker, error::Undecodable, (error::generic::code)filechecker_issue::undecodable::FileOpen, OutputFileName);
                return;
            }
            SizeOnDisk = File_Read.Buffer_Size;
        }
        else
            SizeOnDisk = (size_t)-1;
        Offset = 0;
    }

    // Do we need to write or check?
    if (Offset == (size_t)-1)
        return; // File is flagged as already with wrong data

    // Check hash operation
    if (M->Hashes || M->Hashes_FromRAWcooked || M->Hashes_FromAttachments)
    {
        if (!Mode[IsNotBegin])
        {
            MD5 = new MD5_CTX;
            MD5_Init((MD5_CTX*)MD5);
        }

        CheckMD5(RawFrame);

        if (!Mode[IsNotEnd])
        {
            md5 MD5_Result;
            MD5_Final(MD5_Result.data(), (MD5_CTX*)MD5);

            if (M->Hashes)
                M->Hashes->FromFile(OutputFileName, MD5_Result);
            if (M->Hashes_FromRAWcooked)
                M->Hashes_FromRAWcooked->FromFile(OutputFileName, MD5_Result);
            if (M->Hashes_FromAttachments)
                M->Hashes_FromAttachments->FromFile(OutputFileName, MD5_Result);
        }
    }
                
    // Check file operation
    bool DataIsCheckedAndOk;
    if (File_Read.IsOpen())
    {
        if (!CheckFile(RawFrame))
        {
            if (Mode[IsNotEnd] || Offset == File_Read.Buffer_Size)
                return; // All is OK
            DataIsCheckedAndOk = true;
        }
        else
            DataIsCheckedAndOk = false;

        // Files don't match, decide what we should do (overwrite or don't overwrite, log check error or don't log)
        File_Read.Close();
        bool HasNoError = false;
        if (!Mode[NoWrite] && UserMode)
        {
            user_mode NewMode = *UserMode;
            if (*UserMode == Ask && Ask_Callback)
            {
                NewMode = Ask_Callback(UserMode, OutputFileName, " and is not same", true, M->ProgressIndicator_IsPaused, M->ProgressIndicator_IsEnd);
            }
            if (NewMode == AlwaysYes)
            {
                if (file::return_value Result = File_Write.Open_WriteMode(BaseDirectory, OutputFileName, false))
                {
                    if (Errors)
                        switch (Result)
                        {
                        case file::Error_CreateDirectory:       Errors->Error(IO_FileWriter, error::Undecodable, (error::generic::code)filewriter_issue::undecodable::CreateDirectory, OutputFileName); break;
                        case file::Error_FileCreate:            Errors->Error(IO_FileWriter, error::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileCreate, OutputFileName); break;
                        default:                                Errors->Error(IO_FileWriter, error::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName); break;
                        }
                    Offset = (size_t)-1;
                    return;
                }
                if (Offset)
                {
                    if (file::return_value Result = File_Write.Seek(Offset))
                    {
                        if (Errors)
                            Errors->Error(IO_FileWriter, error::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileSeek, OutputFileName);
                        Offset = (size_t)-1;
                        return;
                    }
                }

                // It is decided to overwrite the file, error "cancelled"
                HasNoError = true;
            }
        }
        if (!HasNoError)
        {
            if (Errors)
                Errors->Error(IO_FileChecker, error::Undecodable, (error::generic::code)filechecker_issue::undecodable::FileComparison, OutputFileName);
            Offset = (size_t)-1;
            return;
        }
    }
    else
        DataIsCheckedAndOk = false;

    // Write file operation
    if (File_Write.IsOpen())
    {
        // Write in the created file or file with wrong data being replaced
        if (!DataIsCheckedAndOk)
        {
            if (WriteFile(RawFrame))
            {
                if (Errors)
                    Errors->Error(IO_FileWriter, error::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName);
                Offset = (size_t)-1;
                return;
            }
        }

        if (!Mode[IsNotEnd])
        {
            // Set end of file if necessary
            if (SizeOnDisk != (size_t)-1 && Offset != SizeOnDisk)
            {
                if (File_Write.SetEndOfFile())
                {
                    if (Errors)
                        Errors->Error(IO_FileWriter, error::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName);
                    Offset = (size_t)-1;
                    return;
                }
            }

            // Close file
            if (File_Write.Close())
            {
                if (Errors)
                    Errors->Error(IO_FileWriter, error::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName);
                Offset = (size_t)-1;
                return;
            }
        }

        return;
    }
}

//---------------------------------------------------------------------------
bool frame_writer::WriteFile(raw_frame* RawFrame)
{
    if (RawFrame->Pre)
    {
        if (File_Write.Write(RawFrame->Pre, RawFrame->Pre_Size))
            return true;
        Offset += RawFrame->Pre_Size;
    }
    if (RawFrame->Buffer)
    {
        if (File_Write.Write(RawFrame->Buffer, RawFrame->Buffer_Size))
            return true;
        Offset += RawFrame->Buffer_Size;
    }
    for (size_t p = 0; p < RawFrame->Planes.size(); p++)
        if (RawFrame->Planes[p]->Buffer)
        {
            if (File_Write.Write(RawFrame->Planes[p]->Buffer, RawFrame->Planes[p]->Buffer_Size))
                return true;
            Offset += RawFrame->Planes[p]->Buffer_Size;
        }
    if (RawFrame->Post)
    {
        if (File_Write.Write(RawFrame->Post, RawFrame->Post_Size))
            return true;
        Offset += RawFrame->Post_Size;
    }

    return false;
}

//---------------------------------------------------------------------------
bool frame_writer::CheckFile(raw_frame* RawFrame)
{
    size_t Offset_Current = Offset;
    if (RawFrame->Pre)
    {
        size_t Offset_After = Offset_Current + RawFrame->Pre_Size;
        if (Offset_Current + Offset_After > File_Read.Buffer_Size)
            return true;
        if (memcmp(File_Read.Buffer + Offset_Current, RawFrame->Pre, RawFrame->Pre_Size))
            return true;
        Offset_Current = Offset_After;
    }
    if (RawFrame->Buffer)
    {
        size_t Offset_After = Offset_Current + RawFrame->Buffer_Size;
        if (Offset_After > File_Read.Buffer_Size)
            return true;
        if (memcmp(File_Read.Buffer + Offset_Current, RawFrame->Buffer, RawFrame->Buffer_Size))
            return true;
        Offset_Current = Offset_After;
    }
    for (size_t p = 0; p < RawFrame->Planes.size(); p++)
        if (RawFrame->Planes[p]->Buffer)
        {
            size_t Offset_After = Offset_Current + RawFrame->Planes[p]->Buffer_Size;
            if (Offset_After > File_Read.Buffer_Size)
                return true;
            if (memcmp(File_Read.Buffer + Offset_Current, RawFrame->Planes[p]->Buffer, RawFrame->Planes[p]->Buffer_Size))
                return true;
            Offset_Current = Offset_After;
        }
    if (RawFrame->Post)
    {
        size_t Offset_After = Offset_Current + RawFrame->Post_Size;
        if (Offset_After > File_Read.Buffer_Size)
            return true;
        if (memcmp(File_Read.Buffer + Offset_Current, RawFrame->Post, RawFrame->Post_Size))
            return true;
        Offset_Current = Offset_After;
    }

    Offset = Offset_Current;
    return false;
}


//---------------------------------------------------------------------------
bool frame_writer::CheckMD5(raw_frame* RawFrame)
{
    if (RawFrame->Pre)
    {
        MD5_Update((MD5_CTX*)MD5, RawFrame->Pre, (unsigned long)RawFrame->Pre_Size);
    }
    if (RawFrame->Buffer)
    {
        MD5_Update((MD5_CTX*)MD5, RawFrame->Buffer, (unsigned long)RawFrame->Buffer_Size);
    }
    for (size_t p = 0; p < RawFrame->Planes.size(); p++)
        if (RawFrame->Planes[p]->Buffer)
        {
            MD5_Update((MD5_CTX*)MD5, RawFrame->Planes[p]->Buffer, (unsigned long)RawFrame->Planes[p]->Buffer_Size);
        }
    if (RawFrame->Post)
    {
        MD5_Update((MD5_CTX*)MD5, RawFrame->Post, (unsigned long)RawFrame->Post_Size);
    }

    return false;
}

//---------------------------------------------------------------------------
FLAC__StreamDecoderReadStatus flac_read_callback(const FLAC__StreamDecoder*, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
    matroska* M = (matroska*)client_data;
    M->FLAC_Read(buffer, bytes);
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}
FLAC__StreamDecoderTellStatus flac_tell_callback(const FLAC__StreamDecoder*, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
    matroska* M = (matroska*)client_data;
    M->FLAC_Tell(absolute_byte_offset);
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}
void flac_metadata_callback(const FLAC__StreamDecoder*, const FLAC__StreamMetadata *metadata, void *client_data)
{
    matroska* M = (matroska*)client_data;
    M->FLAC_Metadata(metadata->data.stream_info.channels, metadata->data.stream_info.bits_per_sample);
}
FLAC__StreamDecoderWriteStatus flac_write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
    matroska* M = (matroska*)client_data;
    M->FLAC_Write((const uint32_t**)buffer, frame->header.blocksize);
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
void flac_error_callback(const FLAC__StreamDecoder *, FLAC__StreamDecoderErrorStatus status, void *)
{
    fprintf(stderr, "Got FLAC error : %s\n", FLAC__StreamDecoderErrorStatusString[status]); // Not expected, should be better handled
}

//---------------------------------------------------------------------------
class flac_info
{
public:
    FLAC__uint64 Pos_Current;
    FLAC__StreamDecoder *Decoder;
    size_t Buffer_Offset_Temp;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint8_t bits_per_sample_Input;
    uint8_t Endianness;
};
void matroska::FLAC_Read(uint8_t buffer[], size_t *bytes)
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];
    size_t SizeMax = Levels[Level].Offset_End - TrackInfo_Current->FlacInfo->Buffer_Offset_Temp;
    if (SizeMax < *bytes)
        *bytes = SizeMax;
    memcpy(buffer, Buffer + TrackInfo_Current->FlacInfo->Buffer_Offset_Temp, *bytes);
    TrackInfo_Current->FlacInfo->Buffer_Offset_Temp += *bytes;
    TrackInfo_Current->FlacInfo->Pos_Current += *bytes;
}
void matroska::FLAC_Tell(uint64_t* absolute_byte_offset)
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];
    *absolute_byte_offset = TrackInfo_Current->FlacInfo->Pos_Current;
}
void matroska::FLAC_Metadata(uint8_t channels, uint8_t bits_per_sample)
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];
    TrackInfo_Current->FlacInfo->channels = channels;
    TrackInfo_Current->FlacInfo->bits_per_sample = bits_per_sample; // Value can be modified later by container information
    TrackInfo_Current->FlacInfo->bits_per_sample_Input = bits_per_sample;
}
void matroska::FLAC_Write(const uint32_t* buffer[], size_t blocksize)
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->Frame.RawFrame->Buffer)
    {
        TrackInfo_Current->Frame.RawFrame->Buffer_Size = 16384 / 8 * TrackInfo_Current->FlacInfo->bits_per_sample*TrackInfo_Current->FlacInfo->channels; // 16384 is the max blocksize in spec
        TrackInfo_Current->Frame.RawFrame->Buffer = new uint8_t[TrackInfo_Current->Frame.RawFrame->Buffer_Size];
        TrackInfo_Current->Frame.RawFrame->Buffer_IsOwned = true;
    }
    uint8_t* Buffer_Current = TrackInfo_Current->Frame.RawFrame->Buffer;

    // Converting libFLAC output to WAV style
    uint8_t channels = TrackInfo_Current->FlacInfo->channels;
    switch (TrackInfo_Current->FlacInfo->bits_per_sample)
    {
        case 8:
                switch (TrackInfo_Current->FlacInfo->bits_per_sample_Input)
                {
                    case 8:
                        switch (TrackInfo_Current->FlacInfo->Endianness)
                        {
                            case 0:
                                for (size_t i = 0; i < blocksize; i++)
                                    for (size_t j = 0; j < channels; j++)
                                    {
                                        *(Buffer_Current++) = (uint8_t)(buffer[j][i]);
                                    }
                                break;
                            case 1:
                                for (size_t i = 0; i < blocksize; i++)
                                    for (size_t j = 0; j < channels; j++)
                                    {
                                        *(Buffer_Current++) = (uint8_t)((buffer[j][i]) + 128);
                                    }
                                break;
                        }
                        break;
                    case 16:
                        switch (TrackInfo_Current->FlacInfo->Endianness)
                        {
                            case 0:
                                for (size_t i = 0; i < blocksize; i++)
                                    for (size_t j = 0; j < channels; j++)
                                    {
                                        *(Buffer_Current++) = (uint8_t)(buffer[j][i] >> 8);
                                    }
                                break;
                            case 1:
                                for (size_t i = 0; i < blocksize; i++)
                                    for (size_t j = 0; j < channels; j++)
                                    {
                                        *(Buffer_Current++) = (uint8_t)((buffer[j][i] >> 8) + 128);
                                    }
                                break;
                        }
                        break;
                }
            break;
        case 16:
            switch (TrackInfo_Current->FlacInfo->Endianness)
            {
                case 0:
                        for (size_t i = 0; i < blocksize; i++)
                            for (size_t j = 0; j < channels; j++)
                            {
                                *(Buffer_Current++) = (uint8_t)(buffer[j][i] >> 8);
                                *(Buffer_Current++) = (uint8_t)(buffer[j][i]);
                            }
                        break;
                case 1:
                        for (size_t i = 0; i < blocksize; i++)
                            for (size_t j = 0; j < channels; j++)
                            {
                                *(Buffer_Current++) = (uint8_t)(buffer[j][i]);
                                *(Buffer_Current++) = (uint8_t)(buffer[j][i] >> 8);
                            }
                        break;
            }
            break;
        case 24:
            switch (TrackInfo_Current->FlacInfo->Endianness)
            {
                case 0:
                        for (size_t i = 0; i < blocksize; i++)
                            for (size_t j = 0; j < channels; j++)
                            {
                                *(Buffer_Current++) = (uint8_t)(buffer[j][i] >> 16);
                                *(Buffer_Current++) = (uint8_t)(buffer[j][i] >> 8);
                                *(Buffer_Current++) = (uint8_t)(buffer[j][i]);
                            }
                        break;
                case 1:
                        for (size_t i = 0; i < blocksize; i++)
                            for (size_t j = 0; j < channels; j++)
                            {
                                *(Buffer_Current++) = (uint8_t)(buffer[j][i]);
                                *(Buffer_Current++) = (uint8_t)(buffer[j][i] >> 8);
                                *(Buffer_Current++) = (uint8_t)(buffer[j][i] >> 16);
                            }
                        break;
            }
            break;
    }

    TrackInfo_Current->Frame.RawFrame->Buffer_Size = Buffer_Current - TrackInfo_Current->Frame.RawFrame->Buffer;

    string OutputFileName = string((const char*)TrackInfo_Current->DPX_FileName[0], TrackInfo_Current->DPX_FileName_Size[0]);
    FormatPath(OutputFileName);

    //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
    TrackInfo_Current->FrameWriter.FrameCall(TrackInfo_Current->Frame.RawFrame, OutputFileName);
}

//---------------------------------------------------------------------------
// Errors

namespace matroska_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "file smaller than expected",
    "Unreadable frame header in reversibility data",
    "More video frames in source content than saved frame headers in reversibility data",
};

enum code : uint8_t
{
    BufferOverflow,
    ReversibilityData_UnreadableFrameHeader,
    ReversibilityData_FrameCount,
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

static_assert(error::Type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

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
ELEMENT_CASE(    7273, Rawcooked_Segment)
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

ELEMENT_BEGIN(Rawcooked_Segment)
ELEMENT_VOID(      70, Rawcooked_Segment_LibraryName)
ELEMENT_VOID(      71, Rawcooked_Segment_LibraryVersion)
ELEMENT_CASE(  726365, Rawcooked_Segment_Erasure)
ELEMENT_END()

ELEMENT_BEGIN(Rawcooked_Segment_Erasure)
ELEMENT_VOID(  726368, Rawcooked_Segment_Erasure_ShardHashes)
ELEMENT_VOID(  726369, Rawcooked_Segment_Erasure_ShardInfo)
ELEMENT_VOID(  72636C, Rawcooked_Segment_Erasure_ParityShardsLocation)
ELEMENT_VOID(  726370, Rawcooked_Segment_Erasure_ParityShards)
ELEMENT_VOID(  726373, Rawcooked_Segment_Erasure_EbmlStartLocation)
ELEMENT_END()

//---------------------------------------------------------------------------
// CRC_32_Table (Little Endian bitstream, )
// The CRC in use is the IEEE-CRC-32 algorithm as used in the ISO 3309 standard and in section 8.1.1.6.2 of ITU-T recommendation V.42, with initial value of 0xFFFFFFFF. The CRC value MUST be computed on a little endian bitstream and MUST use little endian storage.
// A CRC is computed like this:
// Init: uint32_t CRC32 ^= 0;
// for each data byte do
//     CRC32=(CRC32>>8) ^ Mk_CRC32_Table[(CRC32&0xFF)^*Buffer_Current++];
// End: CRC32 ^= 0;
static const uint32_t Matroska_CRC32_Table[256] =
{
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02Ef8D,
};

//---------------------------------------------------------------------------
void Matroska_CRC32_Compute(uint32_t& CRC32, const uint8_t* Buffer_Current, const uint8_t* Buffer_End)
{
    while (Buffer_Current < Buffer_End)
        CRC32 = (CRC32 >> 8) ^ Matroska_CRC32_Table[(CRC32 & 0xFF) ^ *Buffer_Current++];
}

//---------------------------------------------------------------------------
// Glue
void matroska_ProgressIndicator_Show(matroska* M)
{
    M->ProgressIndicator_Show();
}

//---------------------------------------------------------------------------
matroska::matroska(const string& OutputDirectoryName, user_mode* Mode, ask_callback Ask_Callback, errors* Errors_Source) :
    input_base(Errors_Source, Parser_Matroska),
    FramesPool(nullptr),
    FrameWriter_Template(OutputDirectoryName, Mode, Ask_Callback, this, Errors_Source),
    Quiet(false),
    NoWrite(false),
    NoOutputCheck(false),
    Hashes_FromRAWcooked(new hashes(Errors_Source)),
    Hashes_FromAttachments(new hashes(Errors_Source))
{
}

//---------------------------------------------------------------------------
matroska::~matroska()
{
    Shutdown();
    delete Hashes_FromRAWcooked;
    delete Hashes_FromAttachments;
}

//---------------------------------------------------------------------------
void matroska::Shutdown()
{
    if (!FramesPool)
        return;

    for (size_t i = 0; i < TrackInfo.size(); i++)
    {
        trackinfo* TrackInfo_Current = TrackInfo[i];

        if (TrackInfo_Current->Unique)
        {
            TrackInfo_Current->Frame.RawFrame->Buffer = Buffer + Buffer_Offset;
            TrackInfo_Current->Frame.RawFrame->Buffer_Size = 0;
            TrackInfo_Current->Frame.RawFrame->Buffer_IsOwned = false;
            TrackInfo_Current->Frame.RawFrame->Pre = nullptr;
            TrackInfo_Current->Frame.RawFrame->Pre_Size = 0;
            if (TrackInfo_Current->DPX_After && TrackInfo_Current->DPX_After_Size[0])
            {
                TrackInfo_Current->Frame.RawFrame->Post = TrackInfo_Current->DPX_After[0];
                TrackInfo_Current->Frame.RawFrame->Post_Size = TrackInfo_Current->DPX_After_Size[0];
            }
            else
            {
                TrackInfo_Current->Frame.RawFrame->Post = nullptr;
                TrackInfo_Current->Frame.RawFrame->Post_Size = 0;
            }

            string OutputFileName = string((const char*)TrackInfo_Current->DPX_FileName[0], TrackInfo_Current->DPX_FileName_Size[0]);
            FormatPath(OutputFileName);

            //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
            TrackInfo_Current->FrameWriter.Mode[frame_writer::IsNotEnd] = false;
            TrackInfo_Current->FrameWriter.FrameCall(TrackInfo_Current->Frame.RawFrame, OutputFileName);
        }
    }

    if (Hashes_FromRAWcooked)
        Hashes_FromRAWcooked->Finish();
    if (Hashes_FromAttachments)
        Hashes_FromAttachments->Finish();

    FramesPool->shutdown();
    delete FramesPool;
    FramesPool = nullptr;
}

//---------------------------------------------------------------------------
matroska::call matroska::SubElements_Void(uint64_t Name)
{
    Levels[Level].SubElements = &matroska::SubElements_Void; return &matroska::Void;
}

//---------------------------------------------------------------------------
matroska::erasure_encode::erasure_encode(erasure_info& Info_)
{
    Info = Info_;

    size_t DataHashes_Count = (Info.erasureLength + Info.shardSize - 1) / (Info.shardSize);
    size_t ParityHashes_Count = (DataHashes_Count + Info.dataShardCount - 1) / Info.dataShardCount;
    ParityHashes_Count *= Info.parityShardCount;

    RS = new ReedSolomon(Info.dataShardCount, Info.parityShardCount);
    HashValues = new rawcooked::hash_value[DataHashes_Count + ParityHashes_Count];
    ParityShards = new uint8_t[ParityHashes_Count * Info.shardSize];
}

//---------------------------------------------------------------------------
size_t matroska::erasure_encode::Encode(uint8_t* Buffer, size_t Buffer_Max, size_t Offset)
{
    // Init
    uint8_t* Shards[256];
    size_t Package_Pos = Offset / Info.shardSize / Info.dataShardCount;
    size_t Hash_Pos = Package_Pos * (Info.dataShardCount + Info.parityShardCount);
    size_t Parity_Pos = Package_Pos * Info.parityShardCount;

    // Data shards init
    for (uint8_t i = 0; i < Info.dataShardCount; i++)
    {
        size_t Size = Buffer_Max - Offset;
        if (Size > Info.shardSize)
            Size = Info.shardSize;

        if (Size == Info.shardSize)
        {
            Shards[i] = Buffer + Offset;
        }
        else
        {
            Shards[i] = new uint8_t[Info.shardSize];
            memcpy(Shards[i], Buffer + Offset, Size);
            memset(Shards[i] + Size, 0x00, Info.shardSize - Size);
        }

        if (Size)
        {
            HashValues[Hash_Pos++] = Compute_MD5(Buffer + Offset, Size);
            Offset += Size;
        }
    }

    // Parity shards init
    auto ParityShards_Begin = ParityShards + Parity_Pos * Info.shardSize;
    auto ParityShards_Temp = ParityShards_Begin;
    for (uint8_t i = 0; i < Info.parityShardCount; i++)
    {
        Shards[Info.dataShardCount + i] = ParityShards_Temp;
        ParityShards_Temp += Info.shardSize;
    }
    memset(ParityShards_Begin, 0x00, ParityShards_Temp - ParityShards_Begin);

    // Process
    RS->encode(Shards, Info.shardSize);

    // Store hashes
    for (uint8_t i = 0; i < Info.parityShardCount; i++)
    {
        HashValues[Hash_Pos++] = Compute_MD5(Shards[Info.dataShardCount + i], Info.shardSize);
    }

    return Offset;
}

//---------------------------------------------------------------------------
bool matroska::erasure_check::Init()
{
    DataHashes_Count = (Info.erasureLength + Info.shardSize - 1) / (Info.shardSize);
    ParityHashes_Count = (DataHashes_Count + Info.dataShardCount - 1) / Info.dataShardCount;
    ParityHashes_Count *= Info.parityShardCount;
    if (16 * (DataHashes_Count + ParityHashes_Count) != HashValues_Size)
    {
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------
size_t matroska::erasure_check::Check(uint8_t* Buffer, size_t Buffer_Max, size_t Offset)
{
    // Coherency
    size_t isNOK = 0;
    if (HashValues_Size != 16 * (DataHashes_Count + ParityHashes_Count))
    {
        isNOK = 1;
    }
    if (ParityShards_Size != Info.shardSize * ParityHashes_Count)
    {
        if (ParityShards_Size || !Info.shardSize || !ParityHashes_Count)
            isNOK = 1;
        else
            ParityShards_Size = Info.shardSize * ParityHashes_Count;
    }
    if (isNOK)
        return Offset; // TEMP TODO: Issues

    // Init
    size_t Offset_Base = Offset;
    size_t Package_Pos = Offset / Info.shardSize / Info.dataShardCount;
    size_t Hash_Pos = Package_Pos * (Info.dataShardCount + Info.parityShardCount);
    size_t Parity_Pos = Package_Pos * Info.parityShardCount;

    // Compute hash of data shards
    std::vector<bool> shardPresent;
    for (uint8_t i = 0; i < Info.dataShardCount; i++)
    {
        size_t Size = Buffer_Max - Offset;
        if (Size > Info.shardSize)
            Size = Info.shardSize;
        if (!Size)
            break;

        shardPresent.push_back(memcmp(HashValues[Hash_Pos + i].Values, Compute_MD5(Buffer + Offset, Size).Values, 16) ? false : true);
        if (!shardPresent.back())
            isNOK++;
        
        Offset += Size;
    }

    //if (!isNOK)
    //    return Offset; // All is fine

    // Compute hash of parity shards
    size_t dataShardTempCount = shardPresent.size();
    shardPresent.resize(248, true);
    for (uint8_t i = 0; i < Info.parityShardCount; i++)
    {
        shardPresent.push_back(memcmp(HashValues[Hash_Pos + dataShardTempCount + i].Values, Compute_MD5(ParityShards + (Parity_Pos + i) * Info.shardSize, Info.shardSize).Values, 16) ? false : true);
        if (!shardPresent.back())
            isNOK++;
    }

    if (isNOK)
    {
        if (isNOK > Info.parityShardCount)
        {
            if (Errors)
            {
                std::stringstream in, out;
                in << std::hex << Offset_Base;
                out << std::hex << Offset_Base + Info.dataShardCount * Info.shardSize - 1;
                Errors->Error(IO_FileChecker, error::Undecodable, (error::generic::code)filechecker_issue::undecodable::Erasure_TooManyErrors, "at 0x" + in.str() + "-0x" + out.str());
            }

            return (uint64_t)-1;
        }

        uint8_t** Shards = new uint8_t* [Info.dataShardCount + Info.parityShardCount];
        Offset = Offset_Base;
        
        for (uint8_t i = 0; i < Info.dataShardCount; i++)
        {
            size_t Size = Buffer_Max - Offset;
            if (Size > Info.shardSize)
                Size = Info.shardSize;

            if (Size == Info.shardSize)
            {
                Shards[i] = Buffer + Offset;
            }
            else
            {
                Shards[i] = new uint8_t[Info.shardSize];
                memcpy(Shards[i], Buffer + Offset, Size);
                memset(Shards[i] + Size, 0x00, Info.shardSize - Size);
            }

            Offset += Size;
        }

        for (uint8_t i = 0; i < Info.parityShardCount; i++)
        {
            Shards[Info.dataShardCount + i] = ParityShards + (Parity_Pos + i) * Info.shardSize ;
        }

        if (Write)
        {
            ReedSolomon RS(Info.dataShardCount, Info.parityShardCount);
            if (!RS.repair(Shards, Info.shardSize, shardPresent))
            {
                if (Errors)
                {
                    std::stringstream in, out;
                    in << std::hex << Offset_Base;
                    out << std::hex << Offset_Base + Info.dataShardCount * Info.shardSize - 1;
                    Errors->Error(IO_FileChecker, error::Undecodable, (error::generic::code)filechecker_issue::undecodable::Erasure_TooManyErrors, "at 0x" + in.str() + "-0x" + out.str());
                }

                return (uint64_t)-1;
            }
        }
        for (size_t i = 0; i < shardPresent.size(); i++)
            if (!shardPresent[i])
            {
                if (i < Info.dataShardCount) // Data shard
                {
                    size_t Write_Offset = Offset_Base + i * Info.shardSize;
                    size_t Write_Size = Buffer_Max - Write_Offset;
                    if (Write_Size > Info.shardSize)
                        Write_Size = Info.shardSize;

                    if (Write)
                    {
                        memcpy(Buffer + Write_Offset, Shards[i], Write_Size);
                    }
                    else
                    {
                        if (Errors)
                        {
                            std::stringstream in, out;
                            in << std::hex << Write_Offset;
                            out << std::hex << Write_Offset + Write_Size - 1;
                            Errors->Error(IO_FileChecker, error::Invalid, (error::generic::code)filechecker_issue::invalid::Erasure_DataCorrupted, "at 0x" + in.str() + "-0x" + out.str());
                        }
                    }
                }
                else // Parity shard
                {
                    size_t Offset_Temp = (Parity_Pos + i - Info.dataShardCount) * Info.shardSize;

                    if (Write)
                    {
                        memcpy(ParityShards + Offset_Temp, Shards[i], Info.shardSize);
                    }
                    else
                    {
                        if (Errors)
                        {
                            std::stringstream in, out;
                            in << std::hex << ParityShards - Buffer + Offset_Temp;
                            out << std::hex << ParityShards - Buffer + Offset_Temp + Info.shardSize - 1;
                            Errors->Error(IO_FileChecker, error::Invalid, (error::generic::code)filechecker_issue::invalid::Erasure_ParityCorrupted, "at 0x" + in.str() + "-0x" + out.str());
                        }
                    }
                }
            }

        if (!Write)
            return (uint64_t)-1;
    }

    return Offset;
}

//---------------------------------------------------------------------------
rawcooked::hash_value matroska::erasure::Compute_MD5(uint8_t* Buffer, size_t Buffer_Size)
{
    MD5_CTX Hash;
    MD5_Init(&Hash);
    MD5_Update(&Hash, Buffer, (unsigned long)Buffer_Size);
    rawcooked::hash_value HashValue;
    MD5_Final(HashValue.Values, &Hash);
    return HashValue;
}

//---------------------------------------------------------------------------
void matroska::Erasure_Init()
{
    if (Buffer_Size_Matroska)
        return; // Already donc
    Buffer_Size_Matroska = Levels[Level].Offset_End;

    // Erasure encode?
    if (!Erasure_Encode && Actions[Action_Ecc])
    {
        erasure_info Info;
        Info.dataShardCount = 248;
        Info.parityShardCount = 8;
        Info.shardSize = 1024 * 1024;
        Info.erasureStart = 0;
        Info.erasureLength = Buffer_Size_Matroska;

        Erasure_Encode = new erasure_encode(Info);
    }

    // Erasure check?
    if (Buffer_Size_Matroska != Buffer_Size)
    {
        const uint8_t* SearchBuffer = Buffer + Buffer_Size - 4;
        const uint8_t* MatroskaEnd = Buffer + Buffer_Size_Matroska;
        for (; SearchBuffer >= MatroskaEnd; SearchBuffer--)
        {
            if (SearchBuffer[0] != 0x1A || SearchBuffer[1] != 0x45 || SearchBuffer[2] != 0xDF || SearchBuffer[3] != 0xA3)
                continue;
            Buffer_Offset = SearchBuffer - Buffer;
            Levels[Level].Offset_End = Buffer_Offset;
            Matroska_ShouldBeParsed = true;
            break;
        }
    }
}

//---------------------------------------------------------------------------
void matroska::ParseBuffer()
{
    if (Buffer_Size < 4 || Buffer[0] != 0x1A || Buffer[1] != 0x45 || Buffer[2] != 0xDF || Buffer[3] != 0xA3)
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
        FrameWriter_Template.Mode.set(frame_writer::NoWrite);
    if (NoOutputCheck)
        FrameWriter_Template.Mode.set(frame_writer::NoOutputCheck);

    Levels[Level].Offset_End = Buffer_Size;
    Levels[Level].SubElements = &matroska::SubElements__;
    Level++;

    size_t Buffer_Offset_LowerLimit = 0; // Used for indicating the system that we'll not need anymore memory below this value 
    size_t Buffer_Offset_HigherLimit = 0; // Used for indicating the system that we didn't use memory beyond this value
    size_t Erasure_Encode_Offset = 0;
    size_t Erasure_Check_Offset = 0;

    while (Buffer_Offset < Buffer_Size)
    {
        // Erasure encode
        if (Erasure_Encode && Buffer_Size_Matroska && Buffer_Offset + 16 >= Erasure_Encode_Offset && Buffer_Offset < Buffer_Size_Matroska)
        {
            Erasure_Encode_Offset = Erasure_Encode->Encode(Buffer, Buffer_Size_Matroska, Erasure_Encode_Offset);
            if (Buffer_Offset_HigherLimit < Erasure_Encode_Offset)
                Buffer_Offset_HigherLimit = Erasure_Encode_Offset;
        }

        // Erasure check
        if (Erasure_Check && Buffer_Size_Matroska && Buffer_Offset >= Erasure_Check_Offset && Buffer_Offset < Buffer_Size_Matroska)
        {
            Erasure_Check_Offset = Erasure_Check->Check(Buffer, Buffer_Size_Matroska, Erasure_Check_Offset);
            if (Erasure_Check_Offset == (uint64_t)-1)
                break; // Error detected, we stop
            if (Buffer_Offset_HigherLimit < Erasure_Check_Offset)
                Buffer_Offset_HigherLimit = Erasure_Check_Offset;
        }

        // Parse header
        uint64_t Name = Get_EB();
        uint64_t Size = Get_EB();
        if (Size <= Levels[Level - 1].Offset_End - Buffer_Offset)
            Levels[Level].Offset_End = Buffer_Offset + Size;
        else
            Levels[Level].Offset_End = Levels[Level - 1].Offset_End;

        // Erasure encode
        if (Erasure_Encode && Buffer_Size_Matroska && Levels[Level].Offset_End >= Erasure_Encode_Offset && Levels[Level].Offset_End < Buffer_Size_Matroska)
        {
            Erasure_Encode_Offset = Erasure_Encode->Encode(Buffer, Buffer_Size_Matroska, Erasure_Encode_Offset);
            if (Buffer_Offset_HigherLimit < Erasure_Encode_Offset)
                Buffer_Offset_HigherLimit = Erasure_Encode_Offset;
        }

        // Parse data
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
            FileMap->Remap(Actions[Action_Fix]);
            Buffer = FileMap->Buffer;
            Buffer_Offset_LowerLimit = Buffer_Offset;
        }

        if (Matroska_ShouldBeParsed && Buffer_Offset >= Buffer_Size)
        {
            Matroska_ShouldBeParsed = false;
            Buffer_Offset = 0;
            Buffer_Size = Buffer_Size_Matroska;
            Levels[0].Offset_End = Buffer_Size;
            Level = 1;

            if (Erasure_Check)
            {
                if (Erasure_Check->Init())
                {
                    // TODO
                    delete Erasure_Check;
                    Erasure_Check = nullptr;
                }

                if (Erasure_Encode)
                {
                    delete Erasure_Encode;
                    Erasure_Encode = nullptr;
                }
            }
        }
    }

    // Erasure encode
    if (Erasure_Encode)
    {
        Erasure.Erasure(Erasure_Encode->HashValues, Erasure_Encode->ParityShards, Buffer_Size);
        delete[] Erasure_Encode->HashValues;
    }

    // Progress indicator
    Buffer_Offset = Buffer_Size;
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
    Erasure_Init();

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
    AttachedFile_FileName.assign((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
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
    if (!Hashes) // If hashes are provided from elsewhere, they were already tests, not doing the test twice
    {
        hashsum HashSum;
        HashSum.HomePath = AttachedFile_FileName;
        HashSum.List = Hashes_FromAttachments;
        HashSum.Parse(Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
        if (HashSum.IsDetected())
            Hashes_FromAttachments->Ignore(AttachedFile_FileName);
    }

    // Output file
    {
        raw_frame RawFrame;
        RawFrame.Pre = Buffer + Buffer_Offset;
        RawFrame.Pre_Size = Levels[Level].Offset_End - Buffer_Offset;

        //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
        frame_writer FrameWriter(FrameWriter_Template);
        FrameWriter.FrameCall(&RawFrame, AttachedFile_FileName);
    }
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedAttachment()
{
    IsList = true;
    RAWcooked_FileNameIsValid = false;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileHash()
{
    if (!RAWcooked_FileNameIsValid)
        return; // File name should come first. TODO: support when file name comes after
    if (Levels[Level].Offset_End - Buffer_Offset != 17 || Buffer[Buffer_Offset] != 0x80)
        return; // MD5 support only
    Buffer_Offset++;

    array<uint8_t, 16> Hash;
    memcpy(Hash.data(), Buffer + Buffer_Offset, Hash.size());
    Hashes_FromRAWcooked->FromHashFile(AttachedFile_FileName, Hash);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileName()
{
    uint8_t* Output;
    size_t Output_Size;
    Uncompress(Output, Output_Size);
    AttachedFile_FileName.assign((char*)Output, Output_Size);
    delete[] Output; // TODO: avoid new/delete

    RAWcooked_FileNameIsValid = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock()
{
    IsList = true;
    RAWcooked_FileNameIsValid = false;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileHash()
{
    if (!RAWcooked_FileNameIsValid)
        return; // File name should come first. TODO: support when file name comes after
    if (Levels[Level].Offset_End - Buffer_Offset != 17 || Buffer[Buffer_Offset] != 0x80)
        return; // MD5 support only
    Buffer_Offset++;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    TrackInfo_Current->DPX_Buffer_Count--; //TODO: right method for knowing the position

    array<uint8_t, 16> Hash;
    memcpy(Hash.data(), Buffer + Buffer_Offset, Hash.size());
    Hashes_FromRAWcooked->FromHashFile(string((char*)TrackInfo_Current->DPX_FileName[TrackInfo_Current->DPX_Buffer_Count], TrackInfo_Current->DPX_FileName_Size[TrackInfo_Current->DPX_Buffer_Count]), Hash);

    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileName()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_FileName)
    {
        TrackInfo_Current->DPX_FileName = new uint8_t*[1000000];
        memset(TrackInfo_Current->DPX_FileName, 0x00, 1000000 * sizeof(uint8_t*));
        TrackInfo_Current->DPX_FileName_Size = new size_t[1000000];
        memset(TrackInfo_Current->DPX_FileName_Size, 0x00, 1000000 * sizeof(uint64_t));
    }

    Uncompress(TrackInfo_Current->DPX_FileName[TrackInfo_Current->DPX_Buffer_Count], TrackInfo_Current->DPX_FileName_Size[TrackInfo_Current->DPX_Buffer_Count]);
    RAWcooked_FileNameIsValid = true;
}


//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileSize()
{
    uint64_t Size = Levels[Level].Offset_End - Buffer_Offset;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_FileSize)
    {
        TrackInfo_Current->DPX_FileSize = new uint64_t[1000000];
    }

    TrackInfo_Current->DPX_Buffer_Count--; //TODO: right method for knowing the position

    if (!Size)
    {
        TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Count] = (uint64_t)-1; 
        return;
    }

    TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Count] = 0;
    for (; Size; Size--)
    {
        TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Count] <<= 8;
        TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Count] |= Buffer[Buffer_Offset++];
    }

    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_BeforeData()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    TrackInfo_Current->DPX_Buffer_Count--; //TODO: right method for knowing the position

    if (!TrackInfo_Current->DPX_Before)
    {
        TrackInfo_Current->DPX_Before = new uint8_t*[1000000];
        memset(TrackInfo_Current->DPX_Before, 0x00, 1000000 * sizeof(uint8_t*));
        TrackInfo_Current->DPX_Before_Size = new size_t[1000000];
        memset(TrackInfo_Current->DPX_Before_Size, 0x00, 1000000 * sizeof(uint64_t));
    }

    Uncompress(TrackInfo_Current->DPX_Before[TrackInfo_Current->DPX_Buffer_Count], TrackInfo_Current->DPX_Before_Size[TrackInfo_Current->DPX_Buffer_Count]);

    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_AfterData()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    TrackInfo_Current->DPX_Buffer_Count--; //TODO: right method for knowing the position

    if (!TrackInfo_Current->DPX_After)
    {
        TrackInfo_Current->DPX_After = new uint8_t*[1000000];
        memset(TrackInfo_Current->DPX_After, 0x00, 1000000 * sizeof(uint8_t*));
        TrackInfo_Current->DPX_After_Size = new size_t[1000000];
        memset(TrackInfo_Current->DPX_After_Size, 0x00, 1000000 * sizeof(uint64_t));
    }

    Uncompress(TrackInfo_Current->DPX_After[TrackInfo_Current->DPX_Buffer_Count], TrackInfo_Current->DPX_After_Size[TrackInfo_Current->DPX_Buffer_Count]);

    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_InData()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    TrackInfo_Current->DPX_Buffer_Count--; //TODO: right method for knowing the position

    if (!TrackInfo_Current->DPX_In)
    {
        TrackInfo_Current->DPX_In = new uint8_t*[1000000];
        memset(TrackInfo_Current->DPX_In, 0x00, 1000000 * sizeof(uint8_t*));
        TrackInfo_Current->DPX_In_Size = new size_t[1000000];
        memset(TrackInfo_Current->DPX_In_Size, 0x00, 1000000 * sizeof(uint64_t));
    }

    Uncompress(TrackInfo_Current->DPX_In[TrackInfo_Current->DPX_Buffer_Count], TrackInfo_Current->DPX_In_Size[TrackInfo_Current->DPX_Buffer_Count]);

    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionFileName()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_FileName)
    {
        TrackInfo_Current->DPX_FileName = new uint8_t*[1000000];
        memset(TrackInfo_Current->DPX_FileName, 0x00, 1000000 * sizeof(uint8_t*));
        TrackInfo_Current->DPX_FileName_Size = new size_t[1000000];
        memset(TrackInfo_Current->DPX_FileName_Size, 0x00, 1000000 * sizeof(uint64_t));
    }

    Uncompress(TrackInfo_Current->DPX_FileName[TrackInfo_Current->DPX_Buffer_Count], TrackInfo_Current->DPX_FileName_Size[TrackInfo_Current->DPX_Buffer_Count]);

    if (TrackInfo_Current->Mask_FileName)
    {
        for (size_t i = 0; i < TrackInfo_Current->DPX_FileName_Size[TrackInfo_Current->DPX_Buffer_Count] || i < TrackInfo_Current->Mask_FileName_Size; i++)
            TrackInfo_Current->DPX_FileName[TrackInfo_Current->DPX_Buffer_Count][i] += TrackInfo_Current->Mask_FileName[i];
    }

    SanitizeFileName(TrackInfo_Current->DPX_FileName[TrackInfo_Current->DPX_Buffer_Count], TrackInfo_Current->DPX_FileName_Size[TrackInfo_Current->DPX_Buffer_Count]);
    TrackInfo_Current->DPX_Buffer_Count++;
    RAWcooked_FileNameIsValid = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionBeforeData()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    TrackInfo_Current->DPX_Buffer_Count--; //TODO: right method for knowing the position

    if (!TrackInfo_Current->DPX_Before)
    {
        TrackInfo_Current->DPX_Before = new uint8_t*[1000000];
        memset(TrackInfo_Current->DPX_Before, 0x00, 1000000 * sizeof(uint8_t*));
        TrackInfo_Current->DPX_Before_Size = new size_t[1000000];
        memset(TrackInfo_Current->DPX_Before_Size, 0x00, 1000000 * sizeof(uint64_t));
    }

    Uncompress(TrackInfo_Current->DPX_Before[TrackInfo_Current->DPX_Buffer_Count], TrackInfo_Current->DPX_Before_Size[TrackInfo_Current->DPX_Buffer_Count]);

    if (TrackInfo_Current->Mask_Before)
    {
        for (size_t i = 0; i < TrackInfo_Current->DPX_Before_Size[TrackInfo_Current->DPX_Buffer_Count] || i < TrackInfo_Current->Mask_Before_Size; i++)
            TrackInfo_Current->DPX_Before[TrackInfo_Current->DPX_Buffer_Count][i] += TrackInfo_Current->Mask_Before[i];
    }

    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionAfterData()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    TrackInfo_Current->DPX_Buffer_Count--; //TODO: right method for knowing the position

    if (!TrackInfo_Current->DPX_After)
    {
        TrackInfo_Current->DPX_After = new uint8_t*[1000000];
        memset(TrackInfo_Current->DPX_After, 0x00, 1000000 * sizeof(uint8_t*));
        TrackInfo_Current->DPX_After_Size = new size_t[1000000];
        memset(TrackInfo_Current->DPX_After_Size, 0x00, 1000000 * sizeof(uint64_t));
    }

    Uncompress(TrackInfo_Current->DPX_After[TrackInfo_Current->DPX_Buffer_Count], TrackInfo_Current->DPX_After_Size[TrackInfo_Current->DPX_Buffer_Count]);

    if (TrackInfo_Current->Mask_After)
    {
        for (size_t i = 0; i < TrackInfo_Current->DPX_After_Size[TrackInfo_Current->DPX_Buffer_Count] || i < TrackInfo_Current->Mask_After_Size; i++)
            TrackInfo_Current->DPX_After[TrackInfo_Current->DPX_Buffer_Count][i] += TrackInfo_Current->Mask_After[i];
    }

    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionInData()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    TrackInfo_Current->DPX_Buffer_Count--; //TODO: right method for knowing the position

    if (!TrackInfo_Current->DPX_In)
    {
        TrackInfo_Current->DPX_In = new uint8_t*[1000000];
        memset(TrackInfo_Current->DPX_In, 0x00, 1000000 * sizeof(uint8_t*));
        TrackInfo_Current->DPX_In_Size = new size_t[1000000];
        memset(TrackInfo_Current->DPX_In_Size, 0x00, 1000000 * sizeof(uint64_t));
    }

    Uncompress(TrackInfo_Current->DPX_In[TrackInfo_Current->DPX_Buffer_Count], TrackInfo_Current->DPX_In_Size[TrackInfo_Current->DPX_Buffer_Count]);

    if (TrackInfo_Current->Mask_In)
    {
        for (size_t i = 0; i < TrackInfo_Current->DPX_In_Size[TrackInfo_Current->DPX_Buffer_Count] || i < TrackInfo_Current->Mask_In_Size; i++)
            TrackInfo_Current->DPX_In[TrackInfo_Current->DPX_Buffer_Count][i] += TrackInfo_Current->Mask_In[i];
    }

    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedSegment()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryName()
{
    RAWcooked_LibraryName = string((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    RejectIncompatibleVersions();
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryVersion()
{
    RAWcooked_LibraryVersion = string((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    RejectIncompatibleVersions();
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedSegment_PathSeparator()
{
    string RAWcooked_PathSeparator = string((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
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
        TrackInfo.push_back(new trackinfo(FrameWriter_Template));
}


//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_FileHash()
{
    if (!RAWcooked_FileNameIsValid)
        return; // File name should come first. TODO: support when file name comes after
    if (Levels[Level].Offset_End - Buffer_Offset != 17 || Buffer[Buffer_Offset] != 0x80)
        return; // MD5 support only
    Buffer_Offset++;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    array<uint8_t, 16> Hash;
    memcpy(Hash.data(), Buffer + Buffer_Offset, Hash.size());
    Hashes_FromRAWcooked->FromHashFile(string((char*)TrackInfo_Current->DPX_FileName[0], TrackInfo_Current->DPX_FileName_Size[0]), Hash);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_FileName()
{
    if (Levels[Level - 1].Offset_End - Buffer_Offset < 1)
        return;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_FileName)
    {
        TrackInfo_Current->DPX_FileName = new uint8_t*[1];
        TrackInfo_Current->DPX_FileName_Size = new size_t[1];
    }
    TrackInfo_Current->Unique = true;

    Uncompress(TrackInfo_Current->DPX_FileName[0], TrackInfo_Current->DPX_FileName_Size[0]);
    SanitizeFileName(TrackInfo_Current->DPX_FileName[0], TrackInfo_Current->DPX_FileName_Size[0]);
    RAWcooked_FileNameIsValid = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_BeforeData()
{
    if (Levels[Level - 1].Offset_End - Buffer_Offset < 1)
        return;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_Before)
    {
        TrackInfo_Current->DPX_Before = new uint8_t*[1];
        TrackInfo_Current->DPX_Before_Size = new size_t[1];
    }
    TrackInfo_Current->Unique = true;

    Uncompress(TrackInfo_Current->DPX_Before[0], TrackInfo_Current->DPX_Before_Size[0]);

    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_AfterData()
{
    if (Levels[Level - 1].Offset_End - Buffer_Offset < 1)
        return;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_After)
    {
        TrackInfo_Current->DPX_After = new uint8_t*[1];
        TrackInfo_Current->DPX_After_Size = new size_t[1];
    }
    TrackInfo_Current->Unique = true;

    Uncompress(TrackInfo_Current->DPX_After[0], TrackInfo_Current->DPX_After_Size[0]);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_InData()
{
    if (Levels[Level - 1].Offset_End - Buffer_Offset < 1)
        return;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_In)
    {
        TrackInfo_Current->DPX_In = new uint8_t*[1];
        TrackInfo_Current->DPX_In_Size = new size_t[1];
    }
    TrackInfo_Current->Unique = true;

    Uncompress(TrackInfo_Current->DPX_In[0], TrackInfo_Current->DPX_In_Size[0]);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseFileName()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    Uncompress(TrackInfo_Current->Mask_FileName, TrackInfo_Current->Mask_FileName_Size);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseBeforeData()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    Uncompress(TrackInfo_Current->Mask_Before, TrackInfo_Current->Mask_Before_Size);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseAfterData()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    Uncompress(TrackInfo_Current->Mask_After, TrackInfo_Current->Mask_After_Size);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryName()
{
    // Note: LibraryName in RawCookedTrack is out of spec (alpha 1&2)
    RAWcooked_LibraryName = string((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    RejectIncompatibleVersions();
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryVersion()
{
    // Note: LibraryVersion in RawCookedTrack is out of spec (alpha 1&2)
    RAWcooked_LibraryVersion = string((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    RejectIncompatibleVersions();
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseInData()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    Uncompress(TrackInfo_Current->Mask_In, TrackInfo_Current->Mask_In_Size);
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
        trackinfo* TrackInfo_Current = TrackInfo[TrackID - 1];

        // Timestamp
        Block_Timestamp = (Buffer[Buffer_Offset + 1] << 8 || Buffer[Buffer_Offset + 2]);

        // Load balancing between 2 frames (1 is parsed and 1 is written on disk), TODO: better handling
        if (!TrackInfo_Current->R_A)
        {
            TrackInfo_Current->R_A = new raw_frame;
            TrackInfo_Current->R_B = new raw_frame;
        }
        if (TrackInfo_Current->DPX_Buffer_Pos % 2)
            TrackInfo_Current->Frame.RawFrame = TrackInfo_Current->R_B;
        else
            TrackInfo_Current->Frame.RawFrame = TrackInfo_Current->R_A;

        switch (TrackInfo_Current->Format)
        {
            case Format_FFV1:
                            if (TrackInfo_Current->DPX_Before && TrackInfo_Current->DPX_Before_Size[TrackInfo_Current->DPX_Buffer_Pos])
                            {
                                TrackInfo_Current->Frame.RawFrame->Pre = TrackInfo_Current->DPX_Before[TrackInfo_Current->DPX_Buffer_Pos];
                                TrackInfo_Current->Frame.RawFrame->Pre_Size = TrackInfo_Current->DPX_Before_Size[TrackInfo_Current->DPX_Buffer_Pos];
                            }
                            if (TrackInfo_Current->DPX_After && TrackInfo_Current->DPX_After_Size[TrackInfo_Current->DPX_Buffer_Pos])
                            {
                                TrackInfo_Current->Frame.RawFrame->Post = TrackInfo_Current->DPX_After[TrackInfo_Current->DPX_Buffer_Pos];
                                TrackInfo_Current->Frame.RawFrame->Post_Size = TrackInfo_Current->DPX_After_Size[TrackInfo_Current->DPX_Buffer_Pos];
                            }
                            if (TrackInfo_Current->DPX_In && TrackInfo_Current->DPX_In_Size[TrackInfo_Current->DPX_Buffer_Pos])
                            {
                                TrackInfo_Current->Frame.RawFrame->In = TrackInfo_Current->DPX_In[TrackInfo_Current->DPX_Buffer_Pos];
                                TrackInfo_Current->Frame.RawFrame->In_Size = TrackInfo_Current->DPX_In_Size[TrackInfo_Current->DPX_Buffer_Pos];
                            }
                            if (TrackInfo_Current->DPX_Buffer_Pos == 0 && TrackInfo_Current->Frame.RawFrame->Pre)
                            {
                                if (GetFormatAndFlavor(TrackInfo_Current, new dpx(Errors), raw_frame::Flavor_DPX))
                                    if (GetFormatAndFlavor(TrackInfo_Current, new tiff(Errors), raw_frame::Flavor_TIFF))
                                        return;
                            }
                            {
                                TrackInfo_Current->Frame.Read_Buffer_Continue(Buffer + Buffer_Offset + 4, Levels[Level].Offset_End - Buffer_Offset - 4);
                                if (TrackInfo_Current->Frame.RawFrame->Pre && (Actions[Action_Conch] || Actions[Action_Coherency]))
                                    ParseDecodedFrame(TrackInfo_Current);
                                if (TrackInfo_Current->DPX_FileName && TrackInfo_Current->DPX_Buffer_Pos < TrackInfo_Current->DPX_Buffer_Count)
                                {

                                    string OutputFileName = string((const char*)TrackInfo_Current->DPX_FileName[TrackInfo_Current->DPX_Buffer_Pos], TrackInfo_Current->DPX_FileName_Size[TrackInfo_Current->DPX_Buffer_Pos]);
                                    FormatPath(OutputFileName);

                                    //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
                                    TrackInfo_Current->FrameWriter.FrameCall(TrackInfo_Current->Frame.RawFrame, OutputFileName);
                                }
                                else
                                    Undecodable(undecodable::ReversibilityData_FrameCount);
                            }
                            TrackInfo_Current->DPX_Buffer_Pos++;
                            break;
            case Format_FLAC:
                            if (!TrackInfo_Current->FrameWriter.Mode[frame_writer::IsNotBegin])
                            {
                                TrackInfo_Current->FrameWriter.Mode[frame_writer::IsNotEnd] = true;
                                if (TrackInfo_Current->DPX_Before && TrackInfo_Current->DPX_Buffer_Pos < TrackInfo_Current->DPX_Buffer_Count && TrackInfo_Current->DPX_Before_Size[TrackInfo_Current->DPX_Buffer_Pos])
                                {
                                    TrackInfo_Current->Frame.RawFrame->Pre = TrackInfo_Current->DPX_Before[0];
                                    TrackInfo_Current->Frame.RawFrame->Pre_Size = TrackInfo_Current->DPX_Before_Size[0];

                                    if (TrackInfo_Current->Frame.RawFrame->Pre)
                                    {
                                        wav WAV;
                                        WAV.Actions.set(Action_Encode);
                                        WAV.Actions.set(Action_AcceptTruncated);
                                        if (!WAV.Parse(TrackInfo_Current->Frame.RawFrame->Pre, TrackInfo_Current->Frame.RawFrame->Pre_Size))
                                        {
                                            if (!WAV.IsSupported())
                                            {
                                                Undecodable(undecodable::ReversibilityData_UnreadableFrameHeader);
                                                return;
                                            }
                                            TrackInfo_Current->FlacInfo->Endianness = WAV.Endianness();
                                            if (WAV.BitDepth() == 8 && TrackInfo_Current->FlacInfo->bits_per_sample == 16)
                                                TrackInfo_Current->FlacInfo->bits_per_sample = 8; // FFmpeg encoder converts 8-bit input to 16-bit output, forcing 8-bit ouptut in return
                                        }
                                        else
                                        {
                                            aiff AIFF;
                                            AIFF.Actions.set(Action_Encode);
                                            AIFF.Actions.set(Action_AcceptTruncated);
                                            if (!AIFF.Parse(TrackInfo_Current->Frame.RawFrame->Pre, TrackInfo_Current->Frame.RawFrame->Pre_Size))
                                            {
                                                if (!AIFF.IsSupported())
                                                {
                                                    Undecodable(undecodable::ReversibilityData_UnreadableFrameHeader);
                                                    return;
                                                }
                                                TrackInfo_Current->FlacInfo->Endianness = AIFF.Endianness();
                                                if (AIFF.sampleSize() == 8 && TrackInfo_Current->FlacInfo->bits_per_sample == 16)
                                                    TrackInfo_Current->FlacInfo->bits_per_sample = 8; // FFmpeg encoder converts 8-bit input to 16-bit output, forcing 8-bit ouptut in return
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    TrackInfo_Current->Frame.RawFrame->Pre = nullptr;
                                    TrackInfo_Current->Frame.RawFrame->Pre_Size = 0;
                                }
                            }

                            TrackInfo_Current->FlacInfo->Buffer_Offset_Temp = Buffer_Offset + 4;
                            ProcessFrame_FLAC();
                            if (TrackInfo_Current->Unique && !TrackInfo_Current->FrameWriter.Mode[frame_writer::IsNotBegin])
                            {
                                TrackInfo_Current->FrameWriter.Mode[frame_writer::IsNotBegin] = true;
                                if (TrackInfo_Current->Frame.RawFrame->Pre)
                                {
                                    TrackInfo_Current->Frame.RawFrame->Pre = nullptr;
                                    TrackInfo_Current->Frame.RawFrame->Pre_Size = 0;
                                }
                            }
                            break;
            case Format_PCM:
                            if (!TrackInfo_Current->FrameWriter.Mode[frame_writer::IsNotBegin])
                            {
                                TrackInfo_Current->FrameWriter.Mode[frame_writer::IsNotEnd] = true;
                                if (TrackInfo_Current->DPX_Before && TrackInfo_Current->DPX_Buffer_Pos < TrackInfo_Current->DPX_Buffer_Count && TrackInfo_Current->DPX_Before_Size[TrackInfo_Current->DPX_Buffer_Pos])
                                {
                                    TrackInfo_Current->Frame.RawFrame->Pre = TrackInfo_Current->DPX_Before[0];
                                    TrackInfo_Current->Frame.RawFrame->Pre_Size = TrackInfo_Current->DPX_Before_Size[0];
                                }
                                else
                                {
                                    TrackInfo_Current->Frame.RawFrame->Pre = nullptr;
                                    TrackInfo_Current->Frame.RawFrame->Pre_Size = 0;
                                }
                            }
                            TrackInfo_Current->Frame.RawFrame->Buffer = Buffer + Buffer_Offset + 4;
                            TrackInfo_Current->Frame.RawFrame->Buffer_Size = Levels[Level].Offset_End - Buffer_Offset - 4;
                            TrackInfo_Current->Frame.RawFrame->Buffer_IsOwned = false;

                            {
                                string OutputFileName = string((const char*)TrackInfo_Current->DPX_FileName[0], TrackInfo_Current->DPX_FileName_Size[0]);
                                FormatPath(OutputFileName);

                                //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
                                TrackInfo_Current->FrameWriter.FrameCall(TrackInfo_Current->Frame.RawFrame, OutputFileName);
                                if (TrackInfo_Current->Unique && !TrackInfo_Current->FrameWriter.Mode[frame_writer::IsNotBegin])
                                {
                                    TrackInfo_Current->FrameWriter.Mode[frame_writer::IsNotBegin] = true;
                                    if (TrackInfo_Current->Frame.RawFrame->Pre)
                                    {
                                        TrackInfo_Current->Frame.RawFrame->Pre = nullptr;
                                        TrackInfo_Current->Frame.RawFrame->Pre_Size = 0;
                                    }
                                }
                            }
                            break;
                default:;
        }
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
        TrackInfo.push_back(new trackinfo(FrameWriter_Template));
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_Video()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_CodecID()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    string Value((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
    if (Value == "V_MS/VFW/FOURCC")
        TrackInfo_Current->Format = Format_FFV1; // TODO: check CodecPrivate
    if (Value == "A_FLAC")
        TrackInfo_Current->Format = Format_FLAC;
    if (Value == "A_PCM/INT/LIT")
        TrackInfo_Current->Format = Format_PCM;
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_CodecPrivate()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    switch (TrackInfo_Current->Format)
    {
        case Format_FFV1: return ProcessCodecPrivate_FFV1();
        case Format_FLAC: return ProcessCodecPrivate_FLAC();
        default: return;
    }
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_Video_PixelWidth()
{
    uint32_t Data = 0;
    if (Levels[Level].Offset_End - Buffer_Offset == 1)
        Data = ((uint32_t)Buffer[Buffer_Offset]);
    if (Levels[Level].Offset_End - Buffer_Offset == 2)
        Data = (((uint32_t)Buffer[Buffer_Offset]) << 8) | ((uint32_t)Buffer[Buffer_Offset + 1]);

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];
    TrackInfo_Current->Frame.SetWidth(Data);
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_Video_PixelHeight()
{
    uint32_t Data = 0;
    if (Levels[Level].Offset_End - Buffer_Offset == 1)
        Data = ((uint32_t)Buffer[Buffer_Offset]);
    if (Levels[Level].Offset_End - Buffer_Offset == 2)
        Data = (((uint32_t)Buffer[Buffer_Offset]) << 8) | ((uint32_t)Buffer[Buffer_Offset + 1]);

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];
    TrackInfo_Current->Frame.SetHeight(Data);
}

//---------------------------------------------------------------------------
void matroska::Rawcooked_Segment()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Rawcooked_Segment_LibraryName()
{
}

//---------------------------------------------------------------------------
void matroska::Rawcooked_Segment_LibraryVersion()
{
}

//---------------------------------------------------------------------------
void matroska::Rawcooked_Segment_Erasure()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Rawcooked_Segment_Erasure_EbmlStartLocation()
{
    size_t Size = Levels[Level].Offset_End - Buffer_Offset;
    int64_t Data = Get_BXs(Size);
}

//---------------------------------------------------------------------------
void matroska::Rawcooked_Segment_Erasure_ShardHashes()
{
    uint64_t Kind = Get_EB();
    size_t Size = (Levels[Level].Offset_End - Buffer_Offset);
    if (Kind == 0 && !(Size % 16)) //MD5
    {
        if (!Erasure_Check)
            Erasure_Check = new erasure_check(Actions[Action_Fix], Errors);
        Erasure_Check->HashValues_Size = Size;
        Erasure_Check->HashValues = new rawcooked::hash_value[Size / 16];
        memcpy(Erasure_Check->HashValues, Buffer + Buffer_Offset, Size);
        Buffer_Offset += Size;
    }
}

//---------------------------------------------------------------------------
void matroska::Rawcooked_Segment_Erasure_ShardInfo()
{
    uint64_t dataShardCount = Get_EB();
    uint64_t parityShardCount = Get_EB();
    uint64_t shardSize = Get_EB();
    uint64_t erasureStart = Get_EB();
    uint64_t erasureLength = Get_EB();

    if (Buffer_Offset <= Levels[Level].Offset_End
     && dataShardCount && dataShardCount <= 0xFF
     && parityShardCount && parityShardCount <= 0xFF
     && dataShardCount + parityShardCount <= 0x100
     && erasureStart < Buffer_Size
     && erasureLength < Buffer_Size
     && erasureStart < Buffer_Size - erasureLength)
    {
        if (!Erasure_Check)
            Erasure_Check = new erasure_check(Actions[Action_Fix], Errors);
        Erasure_Check->Info.dataShardCount = (uint8_t)dataShardCount;
        Erasure_Check->Info.parityShardCount = (uint8_t)parityShardCount;
        Erasure_Check->Info.shardSize = shardSize;
        Erasure_Check->Info.erasureStart = erasureStart;
        Erasure_Check->Info.erasureLength = erasureLength;
    }
}

//---------------------------------------------------------------------------
void matroska::Rawcooked_Segment_Erasure_ParityShards()
{
    size_t Size = Levels[Level].Offset_End - Buffer_Offset;
    if (!Erasure_Check)
        Erasure_Check = new erasure_check(Actions[Action_Fix], Errors);
    Erasure_Check->ParityShards = Buffer + Buffer_Offset;
    Erasure_Check->ParityShards_Size = Size;
    Buffer_Offset += Size;
}

//---------------------------------------------------------------------------
void matroska::Rawcooked_Segment_Erasure_ParityShardsLocation()
{
    size_t Size = Levels[Level].Offset_End - Buffer_Offset;
    int64_t Data = Get_BXs(Size);

    uint64_t Remaining = Buffer_Size - Buffer_Offset;
    if ((Data <= 0 || (Remaining < INT64_MAX && Data < (int64_t)Remaining)) && (Data >= 0 || (Buffer_Offset >= (uint64_t)-Data)))
    {
        if (!Erasure_Check)
            Erasure_Check = new erasure_check(Actions[Action_Fix], Errors);
        Erasure_Check->ParityShards = Buffer + (size_t) (Buffer_Offset + Data);
    }
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

        size_t ProgressIndicator_New = (size_t)(((float)Buffer_Offset) * ProgressIndicator_Frequency / Buffer_Size);
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
                    ProgressIndicator_New = (size_t)(((float)Buffer_Offset) * ProgressIndicator_Frequency / Buffer_Size);
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
    while (ProgressIndicator_IsEnd.wait_for(Lock, Frequency) == cv_status::timeout, Buffer_Offset != Buffer_Size);

    // Show summary
    steady_clock::time_point Clock_Current = steady_clock::now();
    steady_clock::duration Duration = Clock_Current - Clock_Init;
    float ByteRate = (float)(Buffer_Size) * 1000 / duration_cast<milliseconds>(Duration).count();
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
bool matroska::GetFormatAndFlavor(trackinfo* TrackInfo_Current, input_base_uncompressed* PotentialParser, raw_frame::flavor Flavor)
{
    PotentialParser->Actions.set(Action_Encode);
    PotentialParser->Actions.set(Action_AcceptTruncated);
    unsigned char* Frame_Buffer;
    size_t Frame_Buffer_Size;
    if (TrackInfo_Current->DPX_FileSize && TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Pos] != (uint64_t)-1)
    {
        // TODO: more optimal method without allocation of the full file size and without new/delete
        Frame_Buffer_Size = TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Pos];
        Frame_Buffer = new uint8_t[Frame_Buffer_Size];
        if (TrackInfo_Current->Frame.RawFrame->Pre)
            memcpy(Frame_Buffer, TrackInfo_Current->Frame.RawFrame->Pre, TrackInfo_Current->Frame.RawFrame->Pre_Size);
        if (TrackInfo_Current->Frame.RawFrame->Post)
            memcpy(Frame_Buffer + Frame_Buffer_Size - TrackInfo_Current->Frame.RawFrame->Post_Size, TrackInfo_Current->Frame.RawFrame->Post, TrackInfo_Current->Frame.RawFrame->Post_Size);
    }
    else
    {
        Frame_Buffer = TrackInfo_Current->Frame.RawFrame->Pre;
        Frame_Buffer_Size = TrackInfo_Current->Frame.RawFrame->Pre_Size;
    }
    bool ParseResult = PotentialParser->Parse(Frame_Buffer, Frame_Buffer_Size);
    if (TrackInfo_Current->DPX_FileSize && TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Pos] != (uint64_t)-1)
        delete[] Frame_Buffer;
    if (ParseResult)
    {
        delete PotentialParser;
        return true;
    }

    if (!PotentialParser->IsSupported())
    {
        delete PotentialParser;
        Undecodable(undecodable::ReversibilityData_UnreadableFrameHeader);
        return true;
    }
    TrackInfo_Current->R_A->Flavor = Flavor;
    TrackInfo_Current->R_A->Flavor_Private = PotentialParser->Flavor;
    TrackInfo_Current->R_B->Flavor = Flavor;
    TrackInfo_Current->R_B->Flavor_Private = PotentialParser->Flavor;

    if (Actions[Action_Conch])
        PotentialParser->Actions.set(Action_Conch);
    if (Actions[Action_Coherency])
        PotentialParser->Actions.set(Action_Coherency);
    TrackInfo_Current->DecodedFrameParser = PotentialParser;
    return false;
}

//---------------------------------------------------------------------------
void matroska::ParseDecodedFrame(trackinfo* TrackInfo_Current)
{
    unsigned char* Frame_Buffer;
    size_t Frame_Buffer_Size;
    if (TrackInfo_Current->DPX_FileSize && TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Pos] != (uint64_t)-1)
    {
        // TODO: more optimal method without allocation of the full file size and without new/delete
        Frame_Buffer_Size = TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Pos];
        Frame_Buffer = new uint8_t[Frame_Buffer_Size];
        if (TrackInfo_Current->Frame.RawFrame->Pre)
            memcpy(Frame_Buffer, TrackInfo_Current->Frame.RawFrame->Pre, TrackInfo_Current->Frame.RawFrame->Pre_Size);
        if (TrackInfo_Current->Frame.RawFrame->Post)
            memcpy(Frame_Buffer + Frame_Buffer_Size - TrackInfo_Current->Frame.RawFrame->Post_Size, TrackInfo_Current->Frame.RawFrame->Post, TrackInfo_Current->Frame.RawFrame->Post_Size);
    }
    else
    {
        Frame_Buffer = TrackInfo_Current->Frame.RawFrame->Pre;
        Frame_Buffer_Size = TrackInfo_Current->Frame.RawFrame->Pre_Size;
    }
    bool ParseResult = TrackInfo_Current->DecodedFrameParser->Parse(Frame_Buffer, Frame_Buffer_Size, TrackInfo_Current->Frame.RawFrame->GetTotalSize());
    if (Frame_Buffer != TrackInfo_Current->Frame.RawFrame->Pre)
        delete[] Frame_Buffer;
}

//---------------------------------------------------------------------------
void matroska::Uncompress(uint8_t* &Output, size_t &Output_Size)
{
    uint64_t RealBuffer_Size = Get_EB();
    if (RealBuffer_Size)
    {
        Output_Size = RealBuffer_Size;
        Output = new uint8_t[Output_Size];

        uLongf t = (uLongf)RealBuffer_Size;
        if (uncompress((Bytef*)Output, &t, (const Bytef*)Buffer + Buffer_Offset, (uLong)(Levels[Level].Offset_End - Buffer_Offset))<0)
        {
            delete[] Output;
            Output = nullptr;
            Output_Size = 0;
        }
        if (t != RealBuffer_Size)
        {
            delete[] Output;
            Output = nullptr;
            Output_Size = 0;
        }
    }
    else
    {
        Output_Size = Levels[Level].Offset_End - Buffer_Offset;
        Output = new uint8_t[Output_Size];
        memcpy(Output, Buffer + Buffer_Offset, Output_Size);
    }
}

//---------------------------------------------------------------------------
// Compressed file can holds directory traversal filenames (e.g. ../../evil.sh)
// Not created by the encoder, but a malevolent person could craft such file
// https://snyk.io/research/zip-slip-vulnerability
void matroska::SanitizeFileName(uint8_t* &FileName, size_t &FileName_Size)
{
    // Replace illegal characters (on the target platform) by underscore
    // Note: the output is not exactly as the source content and information about the exact source file name is lost, this is a limitation of the target platform impossible to bypass
    #if defined(_WIN32) || defined(_WINDOWS)
        for (size_t i = 0; i < FileName_Size; i++)
            if (FileName[i] == ':'
             ||(FileName[i] == ' ' && ((i + 1 >= FileName_Size || FileName[i + 1] == '.' || FileName[i + 1] == PathSeparator) || (i == 0 || FileName[i - 1] == PathSeparator)))
             || FileName[i] == '<'
             || FileName[i] == '>'
             || FileName[i] == '|'
             || FileName[i] == '\"'
             || FileName[i] == '?'
             || FileName[i] == '*')
                FileName[i] = '_';
    #endif

    // Trash leading path separator (used for absolute file names) ("///foo/bar" becomes "foo/bar")
    while (FileName_Size && FileName[0] == PathSeparator)
    {
        FileName_Size --;
        memmove(FileName, FileName + 1, FileName_Size);
    }

    // Trash directory traversals ("../../foo../../ba..r/../.." becomes "foo../ba..r")
    for (size_t i = 0; FileName_Size > 1 && i < FileName_Size - 1; i++)
        if ((i == 0 || FileName[i - 1] == PathSeparator) && FileName[i] == '.' && FileName[i+1] == '.' && (i + 2 >= FileName_Size || FileName[i + 2] == PathSeparator))
        {
            size_t Count = 2;
            if (i + 2 < FileName_Size)
                Count++;
            else if (i)
            {
                Count++;
                i--;
            }
            FileName_Size -= Count;
            memmove(FileName + i, FileName + i + Count, FileName_Size - i);
            i--;
        }
}

//---------------------------------------------------------------------------
void matroska::RejectIncompatibleVersions()
{
    if ((RAWcooked_LibraryName == "__RAWCooked__" || RAWcooked_LibraryName == "__RAWcooked__")  && RAWcooked_LibraryVersion == "__NOT FOR PRODUCTION Alpha 1__") // RAWcooked Alpha 1 is not supported
    {
        std::cerr << RAWcooked_LibraryName << "version " << RAWcooked_LibraryVersion << " is not supported, exiting" << std::endl;
        exit(1);
    }
}

//---------------------------------------------------------------------------
void matroska::ProcessCodecPrivate_FFV1()
{
    if (Levels[Level].Offset_End - Buffer_Offset > 0x28)
    {
        uint32_t Size = ((uint32_t)Buffer[Buffer_Offset]) | (((uint32_t)Buffer[Buffer_Offset + 1]) << 8) | (((uint32_t)Buffer[Buffer_Offset + 2]) << 16) | (((uint32_t)Buffer[Buffer_Offset + 3]) << 24);
        if (Size > Levels[Level].Offset_End - Buffer_Offset)
            return; // integrity issue

        if (Buffer[Buffer_Offset + 0x10] != 'F' || Buffer[Buffer_Offset + 0x11] != 'F' || Buffer[Buffer_Offset + 0x12] != 'V' || Buffer[Buffer_Offset + 0x13] != '1')
            return; // Not FFV1

        trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

        TrackInfo_Current->Frame.Read_Buffer_OutOfBand(Buffer + Buffer_Offset + 0x28, Size - 0x28);
    }
}

//---------------------------------------------------------------------------
void matroska::ProcessCodecPrivate_FLAC()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->FlacInfo)
    {
        TrackInfo_Current->FlacInfo = new flac_info;
        TrackInfo_Current->FlacInfo->Pos_Current = 0;
        TrackInfo_Current->FlacInfo->Decoder = FLAC__stream_decoder_new();
        FLAC__stream_decoder_set_md5_checking(TrackInfo_Current->FlacInfo->Decoder, true);
        if (FLAC__stream_decoder_init_stream(TrackInfo_Current->FlacInfo->Decoder, flac_read_callback, 0, flac_tell_callback, 0, 0, flac_write_callback, flac_metadata_callback, flac_error_callback, this) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
        {
            FLAC__stream_decoder_delete(TrackInfo_Current->FlacInfo->Decoder);
            TrackInfo_Current->FlacInfo->Decoder = nullptr;
            return;
        }
    }

    TrackInfo_Current->FlacInfo->Buffer_Offset_Temp = Buffer_Offset;
    ProcessFrame_FLAC();
}

//---------------------------------------------------------------------------
void matroska::ProcessFrame_FLAC()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    for (;;)
    {
        if (!FLAC__stream_decoder_process_single(TrackInfo_Current->FlacInfo->Decoder))
            break;
        FLAC__uint64 Pos;
        if (!FLAC__stream_decoder_get_decode_position(TrackInfo_Current->FlacInfo->Decoder, &Pos))
            break;
        if (Pos == TrackInfo_Current->FlacInfo->Pos_Current)
            break;
    }
}

//---------------------------------------------------------------------------
void matroska::Erasure_Write(const char* FileName)
{
    Erasure.FileName = FileName;
    Erasure.Erasure_AppendToFile();
}
