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
    DPX_Before(NULL),
    DPX_After(NULL),
    DPX_Buffer_Name(NULL),
    DPX_Buffer_Pos(0),
    DPX_Buffer_Count(0),
    RAWcooked_LibraryName_OK(false),
    RAWcooked_LibraryVersion_OK(false),
    R_A(NULL),
    R_B(NULL)
{
    FramesPool = new ThreadPool(1);
    FramesPool->init();
}

//---------------------------------------------------------------------------
matroska::~matroska()
{
    if (FramesPool)
    {
        FramesPool->shutdown();
        delete FramesPool;
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

    DPX_Buffer_Count--; //TODO: right method for knowing the position

    if (!DPX_After)
    {
        DPX_After = new uint8_t*[1000000];
        memset(DPX_After, 0x00, 1000000 * sizeof(uint8_t*));
        DPX_After_Size = new uint64_t[1000000];
        memset(DPX_After_Size, 0x00, 1000000 * sizeof(uint64_t));
    }
    delete[] DPX_After[DPX_Buffer_Count];
    DPX_After_Size[DPX_Buffer_Count] = Levels[Level].Offset_End - Buffer_Offset - 4;
    DPX_After[DPX_Buffer_Count] = new uint8_t[DPX_After_Size[DPX_Buffer_Count]];
    memcpy(DPX_After[DPX_Buffer_Count], Buffer + Buffer_Offset + 4, DPX_After_Size[DPX_Buffer_Count]);
    DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileName()
{
    if (!DPX_Buffer_Name)
    {
        DPX_Buffer_Name = new string[1000000];
    }
    DPX_Buffer_Name[DPX_Buffer_Count] = string((const char*)Buffer + Buffer_Offset, Levels[Level].Offset_End - Buffer_Offset);
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedBlock_BeforeData()
{
    if (Levels[Level - 1].Offset_End - Buffer_Offset < 4 || Buffer[Buffer_Offset + 0] != 0x00 || Buffer[Buffer_Offset + 1] != 0x00 || Buffer[Buffer_Offset + 2] != 0x00 || Buffer[Buffer_Offset + 3] != 0x00)
        return;

    if (!DPX_Before)
    {
        DPX_Before = new uint8_t*[1000000];
        memset(DPX_Before, 0x00, 1000000 * sizeof(uint8_t*));
        DPX_Before_Size = new uint64_t[1000000];
        memset(DPX_Before_Size, 0x00, 1000000 * sizeof(uint64_t));
    }
    delete[] DPX_Before[DPX_Buffer_Count];
    DPX_Before_Size[DPX_Buffer_Count] = Levels[Level].Offset_End - Buffer_Offset - 4;
    DPX_Before[DPX_Buffer_Count] = new uint8_t[DPX_Before_Size[DPX_Buffer_Count]];
    memcpy(DPX_Before[DPX_Buffer_Count], Buffer + Buffer_Offset + 4, DPX_Before_Size[DPX_Buffer_Count]);
    DPX_Buffer_Count++;
}

//---------------------------------------------------------------------------
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack()
{
    IsList = true;
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
    if (Levels[Level].Offset_End - Buffer_Offset > 4 && (Buffer[Buffer_Offset]&0x7F) == 1) // TODO: check the ID and format
    {
        // Load balacing between 2 frames (1 is parsed and 1 is written on disk), TODO: better handling
        if (!R_A)
        {
            if (!RAWcooked_LibraryName_OK || !RAWcooked_LibraryVersion_OK)
                exit(0);
            R_A = new raw_frame;
            R_B = new raw_frame;
        }
        if (DPX_Buffer_Pos % 2)
            Frame.RawFrame = R_B;
        else
            Frame.RawFrame = R_A;

        if (DPX_Before && DPX_Before_Size[DPX_Buffer_Pos])
        {
            Frame.RawFrame->Pre = DPX_Before[DPX_Buffer_Pos];
            Frame.RawFrame->Pre_Size = DPX_Before_Size[DPX_Buffer_Pos];
            if (DPX_Buffer_Pos == 0)
            {
                dpx DPX;
                DPX.Buffer = Frame.RawFrame->Pre;
                DPX.Buffer_Size = Frame.RawFrame->Pre_Size;
                if (DPX.Parse())
                    return;
                R_A->Style_Private = DPX.Style;
                R_B->Style_Private = DPX.Style;
            }

            if (DPX_After && DPX_After_Size[DPX_Buffer_Pos])
            {
                Frame.RawFrame->Post = DPX_After[DPX_Buffer_Pos];
                Frame.RawFrame->Post_Size = DPX_After_Size[DPX_Buffer_Pos];
            }

            DPX_Buffer_Pos++;
        }
        Frame.Read_Buffer_Continue(Buffer + Buffer_Offset + 4, Levels[Level].Offset_End - Buffer_Offset - 4);
        if (WriteFrameCall)
        {
            write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)WriteFrameCall_Opaque;
            WriteToDisk_Data->FileNameDPX = DPX_Buffer_Name[DPX_Buffer_Pos - 1].c_str();

            FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, Frame.RawFrame, WriteFrameCall_Opaque);
        }
    }
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_Video()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_CodecPrivate()
{
    if (Levels[Level].Offset_End - Buffer_Offset > 0x28)
    {
        Frame.Read_Buffer_OutOfBand(Buffer + Buffer_Offset + 0x28, Levels[Level].Offset_End - Buffer_Offset - 0x28);
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
    Frame.SetWidth(Data);
}

//---------------------------------------------------------------------------
void matroska::Segment_Tracks_TrackEntry_Video_PixelHeight()
{
    uint32_t Data = 0;
    if (Levels[Level].Offset_End - Buffer_Offset == 1)
        Data = ((uint32_t)Buffer[Buffer_Offset]);
    if (Levels[Level].Offset_End - Buffer_Offset == 2)
        Data = (((uint32_t)Buffer[Buffer_Offset]) << 8) | ((uint32_t)Buffer[Buffer_Offset + 1]);
    Frame.SetHeight(Data);
}
