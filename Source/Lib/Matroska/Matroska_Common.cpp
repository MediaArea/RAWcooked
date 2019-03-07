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
#include "Lib/RawFrame/RawFrame.h"
#include "Lib/Config.h"
#include "Lib/FileIO.h"
#include <stdlib.h>
#include <cstdio>
#include <thread>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include "ThreadPool.h"
#include "FLAC/stream_decoder.h"
#include "zlib.h"
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
void WriteFrameCall(uint64_t, raw_frame* RawFrame, const string& OutputFileName)
{
    // Post-processing
    if (RawFrame->Buffer && RawFrame->In && RawFrame->Buffer_Size == RawFrame->In_Size)
        for (size_t i = 0; i < RawFrame->In_Size; i++)
            RawFrame->Buffer[i] ^= RawFrame->In[i];
    if (RawFrame->Planes.size() == 1 && RawFrame->Planes[0] && RawFrame->Planes[0]->Buffer && RawFrame->In && RawFrame->Planes[0]->Buffer_Size == RawFrame->In_Size)
        for (size_t i = 0; i < RawFrame->In_Size; i++)
            RawFrame->Planes[0]->Buffer[i] ^= RawFrame->In[i];

    #ifdef _MSC_VER
        #pragma warning(disable:4996)// _CRT_SECURE_NO_WARNINGS
    #endif
    FILE* F = fopen(OutputFileName.c_str(), (RawFrame->Buffer && !RawFrame->Pre) ? "ab" : "wb");
    #ifdef _MSC_VER
        #pragma warning(default:4996)// _CRT_SECURE_NO_WARNINGS
    #endif
    if (!F)
    {
        size_t i = 0;
        for (;;)
        {
            i = OutputFileName.find_first_of("/\\", i+1);
            if (i == (size_t)-1)
                break;
            string t = OutputFileName.substr(0, i);
            if (access(t.c_str(), 0))
            {
                #if defined(_WIN32) || defined(_WINDOWS)
                if (mkdir(t.c_str()))
                #else
                if (mkdir(t.c_str(), 0755))
                #endif
                    exit(0);
            }
        }
        #ifdef _MSC_VER
            #pragma warning(disable:4996)// _CRT_SECURE_NO_WARNINGS
        #endif
        F = fopen(OutputFileName.c_str(), (RawFrame->Buffer && !RawFrame->Pre) ?"ab":"wb");
        #ifdef _MSC_VER
            #pragma warning(default:4996)// _CRT_SECURE_NO_WARNINGS
        #endif
        if (!F)
            return; // TODO: add a warning message that the file can not be open for writing
    }
    if (RawFrame->Pre)
        fwrite(RawFrame->Pre, RawFrame->Pre_Size, 1, F);
    if (RawFrame->Buffer)
        fwrite(RawFrame->Buffer, RawFrame->Buffer_Size, 1, F);
    for (size_t p = 0; p<RawFrame->Planes.size(); p++)
        fwrite(RawFrame->Planes[p]->Buffer, RawFrame->Planes[p]->Buffer_Size, 1, F);
    if (RawFrame->Post)
        fwrite(RawFrame->Post, RawFrame->Post_Size, 1, F);
    fclose(F);
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
    uint8_t Endianess;
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
                        switch (TrackInfo_Current->FlacInfo->Endianess)
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
                        switch (TrackInfo_Current->FlacInfo->Endianess)
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
            switch (TrackInfo_Current->FlacInfo->Endianess)
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
            switch (TrackInfo_Current->FlacInfo->Endianess)
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

    //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
    WriteFrameCall(0x00, TrackInfo_Current->Frame.RawFrame, OutputDirectoryName + OutputFileName);
}
    
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
ELEMENT_CASE(    7273, Segment_Attachments_AttachedFile_FileData_RawCookedSegment)
ELEMENT_CASE(    7274, Segment_Attachments_AttachedFile_FileData_RawCookedTrack)
ELEMENT_CASE(    7262, Segment_Attachments_AttachedFile_FileData_RawCookedBlock)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData_RawCookedBlock)
ELEMENT_VOID(       2, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_AfterData)
ELEMENT_VOID(       1, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_BeforeData)
ELEMENT_VOID(       5, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_InData)
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
matroska::matroska() :
    input_base(),
    FramesPool(NULL),
    Quiet(false)
{
}

//---------------------------------------------------------------------------
matroska::~matroska()
{
    Shutdown();
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
            if (TrackInfo_Current->DPX_After && TrackInfo_Current->DPX_After_Size[0])
            {
                TrackInfo_Current->Frame.RawFrame->Buffer = Buffer + Buffer_Offset;
                TrackInfo_Current->Frame.RawFrame->Buffer_Size = 0;
                TrackInfo_Current->Frame.RawFrame->Buffer_IsOwned = false;
                TrackInfo_Current->Frame.RawFrame->Pre = NULL;
                TrackInfo_Current->Frame.RawFrame->Pre_Size = 0;
                TrackInfo_Current->Frame.RawFrame->Post = TrackInfo_Current->DPX_After[0];
                TrackInfo_Current->Frame.RawFrame->Post_Size = TrackInfo_Current->DPX_After_Size[0];

                {
                    string OutputFileName = string((const char*)TrackInfo_Current->DPX_FileName[0], TrackInfo_Current->DPX_FileName_Size[0]);

                    //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
                    WriteFrameCall(0, TrackInfo_Current->Frame.RawFrame, OutputDirectoryName + OutputFileName);
                }
            }
        }
    }

    FramesPool->shutdown();
    delete FramesPool;
    FramesPool = NULL;
}

//---------------------------------------------------------------------------
matroska::call matroska::SubElements_Void(uint64_t Name)
{
    Levels[Level].SubElements = &matroska::SubElements_Void; return &matroska::Void;
}

//---------------------------------------------------------------------------
bool matroska::ParseBuffer()
{
    if (Buffer_Size < 4 || Buffer[0] != 0x1A || Buffer[1] != 0x45 || Buffer[2] != 0xDF || Buffer[3] != 0xA3)
        return true;

    Buffer_Offset = 0;
    Level = 0;

    // Progress indicator
    Timestampscale = 1000000;
    Cluster_Timestamp = 0;
    Block_Timestamp = 0;
    thread* ProgressIndicator_Thread;
    if (!Quiet)
        ProgressIndicator_Thread=new thread(matroska_ProgressIndicator_Show, this);

    Levels[Level].Offset_End = Buffer_Size;
    Levels[Level].SubElements = &matroska::SubElements__;
    Level++;

    size_t Buffer_Offset_LowerLimit = 0; // Used for indicating the system that we'll not need anymore memory below this value 

    while (Buffer_Offset < Buffer_Size)
    {
        uint64_t Name = Get_EB();
        uint64_t Size = Get_EB();
        Levels[Level].Offset_End = Buffer_Offset + Size;
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
                Levels[Level].SubElements = NULL;
                Level--;
            }
        }

        // Check if we can indicate the system that we'll not need anymore memory below this value, without indicating it too much
        if (Buffer_Offset > Buffer_Offset_LowerLimit + 1024 * 1024) // TODO: when multi-threaded frame decoding is implemented, we need to check that all thread don't need anymore memory below this value 
        {
            FileMap->Remap();
            Buffer_Offset_LowerLimit = Buffer_Offset;
        }
    }

    // Progress indicator
    Buffer_Offset = Buffer_Size;
    if (!Quiet)
    {
        ProgressIndicator_IsEnd.notify_one();
        ProgressIndicator_Thread->join();
        delete ProgressIndicator_Thread;
    }

    return ErrorMessage() ? true : false;
}

//---------------------------------------------------------------------------
void matroska::Void()
{
}

//---------------------------------------------------------------------------
void matroska::Segment()
{
    IsDetected = true;
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

    {
        raw_frame RawFrame;
        RawFrame.Pre = Buffer + Buffer_Offset;
        RawFrame.Pre_Size = Levels[Level].Offset_End - Buffer_Offset;

        //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
        WriteFrameCall(0, &RawFrame, OutputDirectoryName + AttachedFile_FileName);
    }

}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock()
{
    IsList = true;
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
        TrackInfo.push_back(new trackinfo());
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

        // Load balacing between 2 frames (1 is parsed and 1 is written on disk), TODO: better handling
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
                                dpx DPX;
                                DPX.AcceptTruncated = true;
                                if (DPX.Parse(TrackInfo_Current->Frame.RawFrame->Pre, TrackInfo_Current->Frame.RawFrame->Pre_Size))
                                {
                                    Invalid("Unreadable frame header in reversibility data");
                                    return;
                                }
                                if (DPX.IsDetected)
                                {
                                    TrackInfo_Current->R_A->Flavor = raw_frame::Flavor_DPX;
                                    TrackInfo_Current->R_A->Flavor_Private = DPX.Flavor;
                                    TrackInfo_Current->R_B->Flavor = raw_frame::Flavor_DPX;
                                    TrackInfo_Current->R_B->Flavor_Private = DPX.Flavor;
                                }
                                else
                                {
                                    tiff TIFF;
                                    DPX.AcceptTruncated = true;
                                    unsigned char* Frame_Buffer;
                                    size_t Frame_Buffer_Size;
                                    if (TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Pos] == (uint64_t)-1)
                                    {
                                        Frame_Buffer = TrackInfo_Current->Frame.RawFrame->Pre;
                                        Frame_Buffer_Size = TrackInfo_Current->Frame.RawFrame->Pre_Size;
                                    }
                                    else
                                    {
                                        // TODO: more optimal method without allocation of the full file size and without new/delete
                                        Frame_Buffer_Size = TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Pos];
                                        Frame_Buffer = new uint8_t[Frame_Buffer_Size];
                                        memcpy(Frame_Buffer, TrackInfo_Current->Frame.RawFrame->Pre, TrackInfo_Current->Frame.RawFrame->Pre_Size);
                                        memcpy(Frame_Buffer + Frame_Buffer_Size - TrackInfo_Current->Frame.RawFrame->Post_Size, TrackInfo_Current->Frame.RawFrame->Post, TrackInfo_Current->Frame.RawFrame->Post_Size);
                                    }
                                    bool Result = TIFF.Parse(Frame_Buffer, Frame_Buffer_Size);
                                    if (TrackInfo_Current->DPX_FileSize[TrackInfo_Current->DPX_Buffer_Pos] != (uint64_t)-1)
                                        delete[] Frame_Buffer;
                                    if (Result)
                                    {
                                        Invalid("Unreadable frame header in reversibility data");
                                        return;
                                    }
                                    if (TIFF.IsDetected)
                                    {
                                        TrackInfo_Current->R_A->Flavor = raw_frame::Flavor_TIFF;
                                        TrackInfo_Current->R_A->Flavor_Private = TIFF.Flavor;
                                        TrackInfo_Current->R_B->Flavor = raw_frame::Flavor_TIFF;
                                        TrackInfo_Current->R_B->Flavor_Private = TIFF.Flavor;
                                    }
                                }
                            }
                            TrackInfo_Current->Frame.Read_Buffer_Continue(Buffer + Buffer_Offset + 4, Levels[Level].Offset_End - Buffer_Offset - 4);
                            {
                                if (TrackInfo_Current->DPX_FileName && TrackInfo_Current->DPX_Buffer_Pos < TrackInfo_Current->DPX_Buffer_Count)
                                {

                                    string OutputFileName = string((const char*)TrackInfo_Current->DPX_FileName[TrackInfo_Current->DPX_Buffer_Pos], TrackInfo_Current->DPX_FileName_Size[TrackInfo_Current->DPX_Buffer_Pos]);

                                    //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
                                    WriteFrameCall(Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, OutputDirectoryName + OutputFileName);
                                }
                                else
                                    Invalid("More video frames in source content than saved frame headers in reversibility data");
                            }
                            TrackInfo_Current->DPX_Buffer_Pos++;
                            break;
            case Format_FLAC:
                            if (TrackInfo_Current->DPX_Before && TrackInfo_Current->DPX_Before_Size[0])
                            {
                                if (TrackInfo_Current->DPX_Buffer_Pos == 0)
                                {
                                    TrackInfo_Current->Frame.RawFrame->Pre = TrackInfo_Current->DPX_Before[0];
                                    TrackInfo_Current->Frame.RawFrame->Pre_Size = TrackInfo_Current->DPX_Before_Size[0];
                                }
                                else
                                {
                                    TrackInfo_Current->Frame.RawFrame->Pre = NULL;
                                    TrackInfo_Current->Frame.RawFrame->Pre_Size = 0;
                                }

                                if (TrackInfo_Current->DPX_Buffer_Pos == 0 && TrackInfo_Current->Frame.RawFrame->Pre)
                                {
                                    wav WAV;
                                    WAV.AcceptTruncated = true;
                                    WAV.Parse(TrackInfo_Current->Frame.RawFrame->Pre, TrackInfo_Current->Frame.RawFrame->Pre_Size);
                                    if (WAV.ErrorMessage())
                                    {
                                        Invalid("Unreadable frame header in reversibility data");
                                        return;
                                    }
                                    if (WAV.IsDetected)
                                    {
                                        TrackInfo_Current->FlacInfo->Endianess = WAV.Endianess();
                                        if (WAV.BitDepth() == 8 && TrackInfo_Current->FlacInfo->bits_per_sample == 16)
                                            TrackInfo_Current->FlacInfo->bits_per_sample = 8; // FFmpeg encoder converts 8-bit input to 16-bit output, forcing 8-bit ouptut in return
                                    }
                                    else
                                    {
                                        aiff AIFF;
                                        AIFF.AcceptTruncated = true;
                                        AIFF.Parse(TrackInfo_Current->Frame.RawFrame->Pre, TrackInfo_Current->Frame.RawFrame->Pre_Size);
                                        if (AIFF.ErrorMessage())
                                        {
                                            Invalid("Unreadable frame header in reversibility data");
                                            return;
                                        }
                                        if (AIFF.IsDetected)
                                        {
                                            TrackInfo_Current->FlacInfo->Endianess = AIFF.Endianess();
                                            if (AIFF.sampleSize() == 8 && TrackInfo_Current->FlacInfo->bits_per_sample == 16)
                                                TrackInfo_Current->FlacInfo->bits_per_sample = 8; // FFmpeg encoder converts 8-bit input to 16-bit output, forcing 8-bit ouptut in return
                                        }
                                    }
                                }

                                TrackInfo_Current->DPX_Buffer_Pos++;
                            }

                            TrackInfo_Current->FlacInfo->Buffer_Offset_Temp = Buffer_Offset + 4;
                            ProcessFrame_FLAC();
                            break;
            case Format_PCM:
                            if (TrackInfo_Current->DPX_Before && TrackInfo_Current->DPX_Before_Size[0])
                            {
                                if (TrackInfo_Current->DPX_Buffer_Pos == 0)
                                {
                                    TrackInfo_Current->Frame.RawFrame->Pre = TrackInfo_Current->DPX_Before[0];
                                    TrackInfo_Current->Frame.RawFrame->Pre_Size = TrackInfo_Current->DPX_Before_Size[0];
                                }
                                else
                                {
                                    TrackInfo_Current->Frame.RawFrame->Pre = NULL;
                                    TrackInfo_Current->Frame.RawFrame->Pre_Size = 0;
                                }

                                TrackInfo_Current->DPX_Buffer_Pos++;
                            }
                            TrackInfo_Current->Frame.RawFrame->Buffer = Buffer + Buffer_Offset + 4;
                            TrackInfo_Current->Frame.RawFrame->Buffer_Size = Levels[Level].Offset_End - Buffer_Offset - 4;
                            TrackInfo_Current->Frame.RawFrame->Buffer_IsOwned = false;

                            {
                                string OutputFileName = string((const char*)TrackInfo_Current->DPX_FileName[0], TrackInfo_Current->DPX_FileName_Size[0]);

                                //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
                                WriteFrameCall(Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, OutputDirectoryName + OutputFileName);
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
        TrackInfo.push_back(new trackinfo());
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
            cerr << "    "; // Clean up in case there is less content outputed than the previous time

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
    cerr << "                              \n"; // Clean up in case there is less content outputed than the previous time
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
            Output = NULL;
            Output_Size = 0;
        }
        if (t != RealBuffer_Size)
        {
            delete[] Output;
            Output = NULL;
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
    // Use native path separator if it is not "/"
    if (PathSeparator != '/')
    {
        for (size_t i = 0; i < FileName_Size; i++)
            if (FileName[i] == '/')
                FileName[i] = PathSeparator;
    }

    // Replace illegal characters (on the target platform) by underscore
    // Note: the outpout is not exactly as the source content and information about the exact source file name is lost, this is a limitation of the target platform impossible to bypass
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
            TrackInfo_Current->FlacInfo->Decoder = NULL;
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
