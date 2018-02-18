/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Matroska/Matroska_Common.h"
#include "Lib/DPX/DPX.h"
#include "Lib/RawFrame/RawFrame.h"
#include "Lib/Config.h"
#include <stdlib.h>
#include <cstdio>
#include <thread>
#include "ThreadPool.h"
//---------------------------------------------------------------------------

extern const char* LibraryName;
extern const char* LibraryVersion;

//---------------------------------------------------------------------------
static int Get_EB(unsigned char* Buffer, uint64_t& Offset, uint64_t& Name, uint64_t& Size)
{
    Name = Buffer[Offset];
    uint64_t s = 0;
    while (!(Name&(((uint64_t)1) << (7 - s))))
        s++;
    Name ^= (((uint64_t)1) << (7 - s));
    while (s)
    {
        Name <<= 8;
        Offset++;
        s--;
        Name |= Buffer[Offset];
    }
    Offset++;

    Size = Buffer[Offset];
    s = 0;
    while (!(Size&(((uint64_t)1) << (7 - s))))
        s++;
    Size ^= (((uint64_t)1) << (7 - s));
    while (s)
    {
        Size <<= 8;
        Offset++;
        s--;
        Size |= Buffer[Offset];
    }
    Offset++;

    return 1;
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
ELEMENT_CASE(    7274, Segment_Attachments_AttachedFile_FileData_RawCookedTrack)
ELEMENT_CASE(    7262, Segment_Attachments_AttachedFile_FileData_RawCookedBlock)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData_RawCookedBlock)
ELEMENT_VOID(       2, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_AfterData)
ELEMENT_VOID(       1, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_BeforeData)
ELEMENT_VOID(      10, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileName)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData_RawCookedTrack)
ELEMENT_VOID(       2, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_AfterData)
ELEMENT_VOID(       1, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_BeforeData)
ELEMENT_VOID(      10, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_FileName)
ELEMENT_VOID(      70, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryName)
ELEMENT_VOID(      71, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryVersion)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Cluster)
ELEMENT_VOID(      23, Segment_Cluster_SimpleBlock)
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
matroska::matroska() :
    WriteFrameCall(NULL),
    WriteFrameCall_Opaque(NULL),
    IsDetected(false),
    RAWcooked_LibraryName_OK(false),
    RAWcooked_LibraryVersion_OK(false)
{
    FramesPool = new ThreadPool(1);
    FramesPool->init();
}

//---------------------------------------------------------------------------
matroska::~matroska()
{
    Shutdown();
}

//---------------------------------------------------------------------------
void matroska::Shutdown()
{
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
                TrackInfo_Current->Frame.RawFrame->Post = NULL;
                TrackInfo_Current->Frame.RawFrame->Post = TrackInfo_Current->DPX_After[0];
                TrackInfo_Current->Frame.RawFrame->Post_Size = TrackInfo_Current->DPX_After_Size[0];

                if (WriteFrameCall)
                {
                    write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)WriteFrameCall_Opaque;
                    WriteToDisk_Data->FileNameDPX = TrackInfo_Current->DPX_Buffer_Name[0].c_str();

                    //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
                    WriteFrameCall(0, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque);
                }
            }
        }
    }
    TrackInfo.clear();

    if (FramesPool)
    {
        FramesPool->shutdown();
        delete FramesPool;
        FramesPool = NULL;
    }
}

//---------------------------------------------------------------------------
matroska::call matroska::SubElements_Void(uint64_t Name)
{
    Levels[Level].SubElements = &matroska::SubElements_Void; return &matroska::Void;
}

//---------------------------------------------------------------------------
void matroska::Parse()
{
    if (Buffer_Size < 4 || Buffer[0] != 0x1A || Buffer[1] != 0x45 || Buffer[2] != 0xDF || Buffer[3] != 0xA3)
        return;

    IsDetected = true;
    Buffer_Offset = 0;
    Level = 0;

    Levels[Level].Offset_End = Buffer_Size;
    Levels[Level].SubElements = &matroska::SubElements__;
    Level++;

    while (Buffer_Offset < Buffer_Size)
    {
        uint64_t Name, Size;
        Get_EB(Buffer, Buffer_Offset, Name, Size);
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
            while (Buffer_Offset >= Levels[Level - 1].Offset_End)
            {
                Levels[Level].SubElements = NULL;
                Level--;
            }
        }
    }
}

//---------------------------------------------------------------------------
void matroska::Void()
{
}

//---------------------------------------------------------------------------
void matroska::Segment()
{
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
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileName()
{
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData()
{
    if (Levels[Level - 1].Offset_End - Buffer_Offset < 3 || Buffer[Buffer_Offset + 0] != 0x20 || Buffer[Buffer_Offset + 1] != 0x72 || ( Buffer[Buffer_Offset + 2] != 0x62 && Buffer[Buffer_Offset + 2] != 0x74))
        return;

    IsList = true;

    TrackInfo_Pos = (size_t)-1;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_AfterData()
{
    if (Levels[Level - 1].Offset_End - Buffer_Offset < 4 || Buffer[Buffer_Offset + 0] != 0x00 || Buffer[Buffer_Offset + 1] != 0x00 || Buffer[Buffer_Offset + 2] != 0x00 || Buffer[Buffer_Offset + 3] != 0x00)
        return;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    TrackInfo_Current->DPX_Buffer_Count--; //TODO: right method for knowing the position

    if (!TrackInfo_Current->DPX_After)
    {
        TrackInfo_Current->DPX_After = new uint8_t*[1000000];
        memset(TrackInfo_Current->DPX_After, 0x00, 1000000 * sizeof(uint8_t*));
        TrackInfo_Current->DPX_After_Size = new uint64_t[1000000];
        memset(TrackInfo_Current->DPX_After_Size, 0x00, 1000000 * sizeof(uint64_t));
    }
    delete[] TrackInfo_Current->DPX_After[TrackInfo_Current->DPX_Buffer_Count];
    TrackInfo_Current->DPX_After_Size[TrackInfo_Current->DPX_Buffer_Count] = Levels[Level].Offset_End - Buffer_Offset - 4;
    TrackInfo_Current->DPX_After[TrackInfo_Current->DPX_Buffer_Count] = new uint8_t[TrackInfo_Current->DPX_After_Size[TrackInfo_Current->DPX_Buffer_Count]];
    memcpy(TrackInfo_Current->DPX_After[TrackInfo_Current->DPX_Buffer_Count], Buffer + Buffer_Offset + 4, TrackInfo_Current->DPX_After_Size[TrackInfo_Current->DPX_Buffer_Count]);
    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileName()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_Buffer_Name)
    {
        TrackInfo_Current->DPX_Buffer_Name = new string[1000000];
    }
    TrackInfo_Current->DPX_Buffer_Name[TrackInfo_Current->DPX_Buffer_Count] = string((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_BeforeData()
{
    if (Levels[Level - 1].Offset_End - Buffer_Offset < 4 || Buffer[Buffer_Offset + 0] != 0x00 || Buffer[Buffer_Offset + 1] != 0x00 || Buffer[Buffer_Offset + 2] != 0x00 || Buffer[Buffer_Offset + 3] != 0x00)
        return;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_Before)
    {
        TrackInfo_Current->DPX_Before = new uint8_t*[1000000];
        memset(TrackInfo_Current->DPX_Before, 0x00, 1000000 * sizeof(uint8_t*));
        TrackInfo_Current->DPX_Before_Size = new uint64_t[1000000];
        memset(TrackInfo_Current->DPX_Before_Size, 0x00, 1000000 * sizeof(uint64_t));
    }
    delete[] TrackInfo_Current->DPX_Before[TrackInfo_Current->DPX_Buffer_Count];
    TrackInfo_Current->DPX_Before_Size[TrackInfo_Current->DPX_Buffer_Count] = Levels[Level].Offset_End - Buffer_Offset - 4;
    TrackInfo_Current->DPX_Before[TrackInfo_Current->DPX_Buffer_Count] = new uint8_t[TrackInfo_Current->DPX_Before_Size[TrackInfo_Current->DPX_Buffer_Count]];
    memcpy(TrackInfo_Current->DPX_Before[TrackInfo_Current->DPX_Buffer_Count], Buffer + Buffer_Offset + 4, TrackInfo_Current->DPX_Before_Size[TrackInfo_Current->DPX_Buffer_Count]);
    TrackInfo_Current->DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack()
{
    IsList = true;

    TrackInfo_Pos++;
    if (TrackInfo_Pos >= TrackInfo.size())
        TrackInfo.push_back(new trackinfo());
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_AfterData()
{
    if (Levels[Level - 1].Offset_End - Buffer_Offset < 4 || Buffer[Buffer_Offset + 0] != 0x00 || Buffer[Buffer_Offset + 1] != 0x00 || Buffer[Buffer_Offset + 2] != 0x00 || Buffer[Buffer_Offset + 3] != 0x00)
        return;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_After)
    {
        TrackInfo_Current->DPX_After = new uint8_t*[1];
        TrackInfo_Current->DPX_After_Size = new uint64_t[1];
    }
    TrackInfo_Current->DPX_After_Size[0] = Levels[Level].Offset_End - Buffer_Offset - 4;
    TrackInfo_Current->DPX_After[0] = new uint8_t[TrackInfo_Current->DPX_After_Size[0]];
    memcpy(TrackInfo_Current->DPX_After[0], Buffer + Buffer_Offset + 4, TrackInfo_Current->DPX_After_Size[0]);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_BeforeData()
{
    if (Levels[Level - 1].Offset_End - Buffer_Offset < 4 || Buffer[Buffer_Offset + 0] != 0x00 || Buffer[Buffer_Offset + 1] != 0x00 || Buffer[Buffer_Offset + 2] != 0x00 || Buffer[Buffer_Offset + 3] != 0x00)
        return;

    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_Before)
    {
        TrackInfo_Current->DPX_Before = new uint8_t*[1];
        TrackInfo_Current->DPX_Before_Size = new uint64_t[1];
    }
    TrackInfo_Current->DPX_Before_Size[0] = Levels[Level].Offset_End - Buffer_Offset - 4;
    TrackInfo_Current->DPX_Before[0] = new uint8_t[TrackInfo_Current->DPX_Before_Size[0]];
    memcpy(TrackInfo_Current->DPX_Before[0], Buffer + Buffer_Offset + 4, TrackInfo_Current->DPX_Before_Size[0]);
    TrackInfo_Current->Unique = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_FileName()
{
    trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

    if (!TrackInfo_Current->DPX_Buffer_Name)
    {
        TrackInfo_Current->DPX_Buffer_Name = new string[1];
    }
    TrackInfo_Current->DPX_Buffer_Name[TrackInfo_Current->DPX_Buffer_Count] = string((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryName()
{
    string LibraryName_Software(LibraryName);
    string LibraryName_File((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);

    if (LibraryName_Software != LibraryName_File)
    {
        exit(0);
    }
    RAWcooked_LibraryName_OK = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryVersion()
{
    string LibraryVersion_Software(LibraryVersion);
    string LibraryVersion_File((const char*)Buffer+Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);

    if (LibraryVersion_Software != LibraryVersion_File)
    {
        exit(0);
    }
    RAWcooked_LibraryVersion_OK = true;
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
        trackinfo* TrackInfo_Current = TrackInfo[TrackID - 1];

        // Load balacing between 2 frames (1 is parsed and 1 is written on disk), TODO: better handling
        if (!TrackInfo_Current->R_A)
        {
            if (!RAWcooked_LibraryName_OK || !RAWcooked_LibraryVersion_OK)
                exit(0);
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
                                if (TrackInfo_Current->DPX_Buffer_Pos == 0)
                                {
                                    dpx DPX;
                                    DPX.Buffer = TrackInfo_Current->Frame.RawFrame->Pre;
                                    DPX.Buffer_Size = TrackInfo_Current->Frame.RawFrame->Pre_Size;
                                    if (DPX.Parse())
                                        return;
                                    TrackInfo_Current->R_A->Style_Private = DPX.Style;
                                    TrackInfo_Current->R_B->Style_Private = DPX.Style;
                                }

                                if (TrackInfo_Current->DPX_After && TrackInfo_Current->DPX_After_Size[TrackInfo_Current->DPX_Buffer_Pos])
                                {
                                    TrackInfo_Current->Frame.RawFrame->Post = TrackInfo_Current->DPX_After[TrackInfo_Current->DPX_Buffer_Pos];
                                    TrackInfo_Current->Frame.RawFrame->Post_Size = TrackInfo_Current->DPX_After_Size[TrackInfo_Current->DPX_Buffer_Pos];
                                }

                                TrackInfo_Current->DPX_Buffer_Pos++;
                            }
                            TrackInfo_Current->Frame.Read_Buffer_Continue(Buffer + Buffer_Offset + 4, Levels[Level].Offset_End - Buffer_Offset - 4);
                            if (WriteFrameCall)
                            {
                                write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)WriteFrameCall_Opaque;
                                WriteToDisk_Data->FileNameDPX = TrackInfo_Current->DPX_Buffer_Name[TrackInfo_Current->DPX_Buffer_Pos - 1].c_str();

                                //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
                                WriteFrameCall(Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque);
                            }
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

                            if (WriteFrameCall)
                            {
                                write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)WriteFrameCall_Opaque;
                                WriteToDisk_Data->FileNameDPX = TrackInfo_Current->DPX_Buffer_Name[0].c_str();

                                //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
                                WriteFrameCall(Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque);
                            }
                            break;
                default:;
        }
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
    if (Value == "A_PCM/INT/LIT")
        TrackInfo_Current->Format = Format_PCM;
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_CodecPrivate()
{
    if (Levels[Level].Offset_End - Buffer_Offset > 0x28)
    {
        trackinfo* TrackInfo_Current = TrackInfo[TrackInfo_Pos];

        TrackInfo_Current->Frame.Read_Buffer_OutOfBand(Buffer + Buffer_Offset + 0x28, Levels[Level].Offset_End - Buffer_Offset - 0x28);
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
// Errors
//***************************************************************************

//---------------------------------------------------------------------------
const char* matroska::ErrorMessage()
{
    for (size_t i = 0; i < TrackInfo.size(); i++)
        if (TrackInfo[i]->Frame.ErrorMessage())
            return TrackInfo[i]->Frame.ErrorMessage();

    return NULL;
}
