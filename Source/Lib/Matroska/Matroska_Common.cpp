/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
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
#include <iostream>
#include "ThreadPool.h"
#include "zlib.h"
//---------------------------------------------------------------------------

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

//---------------------------------------------------------------------------
static int Get_EB(unsigned char* Buffer, uint64_t& Offset, uint64_t& Size)
{
    Size = Buffer[Offset];
    uint64_t s = 0;
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
ELEMENT_CASE(    7273, Segment_Attachments_AttachedFile_FileData_RawCookedSegment)
ELEMENT_CASE(    7274, Segment_Attachments_AttachedFile_FileData_RawCookedTrack)
ELEMENT_CASE(    7262, Segment_Attachments_AttachedFile_FileData_RawCookedBlock)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData_RawCookedBlock)
ELEMENT_VOID(       2, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_AfterData)
ELEMENT_VOID(       1, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_BeforeData)
ELEMENT_VOID(      10, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileName)
ELEMENT_VOID(       4, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionAfterData)
ELEMENT_VOID(       3, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionBeforeData)
ELEMENT_VOID(      11, Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionFileName)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData_RawCookedSegment)
ELEMENT_VOID(      70, Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryName)
ELEMENT_VOID(      71, Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryVersion)
ELEMENT_END()

ELEMENT_BEGIN(Segment_Attachments_AttachedFile_FileData_RawCookedTrack)
ELEMENT_VOID(       2, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_AfterData)
ELEMENT_VOID(       1, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_BeforeData)
ELEMENT_VOID(      10, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_FileName)
ELEMENT_VOID(      70, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryName)
ELEMENT_VOID(      71, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryVersion)
ELEMENT_VOID(       4, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseAfterData)
ELEMENT_VOID(       3, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseBeforeData)
ELEMENT_VOID(      11, Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseFileName)
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
    IsDetected(false)
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
                TrackInfo_Current->Frame.RawFrame->Post = NULL;
                TrackInfo_Current->Frame.RawFrame->Post = TrackInfo_Current->DPX_After[0];
                TrackInfo_Current->Frame.RawFrame->Post_Size = TrackInfo_Current->DPX_After_Size[0];

                if (WriteFrameCall)
                {
                    write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)WriteFrameCall_Opaque;
                    WriteToDisk_Data->FileNameDPX = string((const char*)TrackInfo_Current->DPX_FileName[0], TrackInfo_Current->DPX_FileName_Size[0]);

                    //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
                    WriteFrameCall(0, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque);
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
void matroska::Parse()
{
    if (Buffer_Size < 4 || Buffer[0] != 0x1A || Buffer[1] != 0x45 || Buffer[2] != 0xDF || Buffer[3] != 0xA3)
        return;

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
            while (Level && Buffer_Offset >= Levels[Level - 1].Offset_End)
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

    if (WriteFrameCall)
    {
        write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)WriteFrameCall_Opaque;
        WriteToDisk_Data->FileNameDPX = AttachedFile_FileName;

        raw_frame RawFrame;
        RawFrame.Pre = Buffer + Buffer_Offset;
        RawFrame.Pre_Size = Levels[Level].Offset_End - Buffer_Offset;

        //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
        WriteFrameCall(0, &RawFrame, WriteFrameCall_Opaque);
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
void matroska::Segment_Attachments_AttachedFile_FileData_RawCookedTrack()
{
    IsList = true;

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
                            if (TrackInfo_Current->DPX_Buffer_Pos == 0)
                            {
                                dpx DPX;
                                DPX.Buffer = TrackInfo_Current->Frame.RawFrame->Pre;
                                DPX.Buffer_Size = TrackInfo_Current->Frame.RawFrame->Pre_Size;
                                if (DPX.Parse())
                                {
                                    if (TrackInfo_Current->ErrorMessage.empty())
                                    {
                                        TrackInfo_Current->ErrorMessage = "Unreadable frame header in reversibility data";
                                    }
                                    return;
                                }
                                TrackInfo_Current->R_A->Style_Private = DPX.Style;
                                TrackInfo_Current->R_B->Style_Private = DPX.Style;
                            }
                            TrackInfo_Current->Frame.Read_Buffer_Continue(Buffer + Buffer_Offset + 4, Levels[Level].Offset_End - Buffer_Offset - 4);
                            if (WriteFrameCall)
                            {
                                if (TrackInfo_Current->DPX_FileName && TrackInfo_Current->DPX_Buffer_Pos < TrackInfo_Current->DPX_Buffer_Count)
                                {
                                    write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)WriteFrameCall_Opaque;

                                    WriteToDisk_Data->FileNameDPX = string((const char*)TrackInfo_Current->DPX_FileName[TrackInfo_Current->DPX_Buffer_Pos], TrackInfo_Current->DPX_FileName_Size[TrackInfo_Current->DPX_Buffer_Pos]);

                                    //FramesPool->submit(WriteFrameCall, Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque); //TODO: looks like there is some issues with threads and small tasks
                                    WriteFrameCall(Buffer[Buffer_Offset] & 0x7F, TrackInfo_Current->Frame.RawFrame, WriteFrameCall_Opaque);
                                }
                                else if (TrackInfo_Current->ErrorMessage.empty())
                                {
                                    TrackInfo_Current->ErrorMessage = "More video frames in source content than saved frame headers in reversibility data";
                                }
                            }
                            TrackInfo_Current->DPX_Buffer_Pos++;
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
                                WriteToDisk_Data->FileNameDPX = string((const char*)TrackInfo_Current->DPX_FileName[0], TrackInfo_Current->DPX_FileName_Size[0]);

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
    {
        if (TrackInfo[i]->Frame.ErrorMessage())
            return TrackInfo[i]->Frame.ErrorMessage();
        if (!TrackInfo[i]->ErrorMessage.empty())
            return TrackInfo[i]->ErrorMessage.c_str();
    }

    return NULL;
}

//***************************************************************************
// Utils
//***************************************************************************

//---------------------------------------------------------------------------
void matroska::Uncompress(uint8_t* &Output, size_t &Output_Size)
{
    uint64_t RealBuffer_Size;
    Get_EB(Buffer, Buffer_Offset, RealBuffer_Size);
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
void matroska::RejectIncompatibleVersions()
{
    if ((RAWcooked_LibraryName == "__RAWCooked__" || RAWcooked_LibraryName == "__RAWcooked__")  && RAWcooked_LibraryVersion == "__NOT FOR PRODUCTION Alpha 1__") // RAWcooked Alpha 1 is not supported
    {
        std::cerr << RAWcooked_LibraryName << "version " << RAWcooked_LibraryVersion << " is not supported, exiting" << std::endl;
        exit(1);
    }
}
