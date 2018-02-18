/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/RAWcooked/RAWcooked.h"
#include "Lib/Config.h"
#include <cstring>
using namespace std;
//---------------------------------------------------------------------------

// Top level
static const uint32_t Name_RawCookedTrack = 0x7274; // " rt", RAWcooked Track part
static const uint32_t Name_RawCookedBlock = 0x7262; // " rb", RAWcooked BlockGroup part
// File data
static const uint32_t Name_RawCooked_BeforeData = 0x01;
static const uint32_t Name_RawCooked_AfterData = 0x02;
static const uint32_t Name_RawCooked_BeforeData_Mask = 0x03; // Invalid in BlockGroup
static const uint32_t Name_RawCooked_AfterData_Mask = 0x04; // Invalid in BlockGroup
static const uint32_t Name_RawCooked_BeforeData_AddToMask = 0x05; // Invalid in Track
static const uint32_t Name_RawCooked_AfterData_AddToMask = 0x06; // Invalid in Track
// File metadata
static const uint32_t Name_RawCooked_FileName = 0x10;
// File stats
static const uint32_t Name_RawCooked_FileMD5 = 0x20;
static const uint32_t Name_RawCooked_FileSHA1 = 0x21;
static const uint32_t Name_RawCooked_FileSHA256 = 0x21;
// Global information
static const uint32_t Name_RawCooked_LibraryName = 0x70;
static const uint32_t Name_RawCooked_LibraryVersion = 0x71;

extern const char* LibraryName = "__RAWcooked__";
extern const char* LibraryVersion = "__NOT FOR PRODUCTION Alpha 1__";

//---------------------------------------------------------------------------
static size_t Size_EB(uint32_t Name, uint64_t Size)
{
    size_t N_l = 1;
    while (Name >> (N_l * 7))
        N_l++;

    size_t S_l = 1;
    while (Size >> (S_l * 7))
        S_l++;

    return N_l + S_l + Size;
}

//---------------------------------------------------------------------------
static void Put_EB(unsigned char* Buffer, size_t& Offset, uint32_t Name, uint64_t Size)
{
    size_t N_l = 1;
    while (Name >> (N_l * 7))
        N_l++;
    uint32_t N2 = Name | (((uint32_t)1) << (N_l * 7));
    while (N_l)
    {
        Buffer[Offset] = (uint8_t)(N2 >> ((N_l - 1)*8));
        Offset++;
        N_l--;
    }

    size_t S_l = 1;
    while (Size >> (S_l * 7))
        S_l++;
    uint64_t S2 = Size | (((uint64_t)1) << (S_l * 7));
    while (S_l)
    {
        Buffer[Offset] = (uint8_t)(S2 >> ((S_l - 1)*8));
        Offset++;
        S_l--;
    }
}

//---------------------------------------------------------------------------
rawcooked::rawcooked() :
    Unique(false),
    WriteFileCall(NULL),
    WriteFileCall_Opaque(NULL)
{
}

//---------------------------------------------------------------------------
void rawcooked::Parse()
{
    write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)WriteFileCall_Opaque;

    uint64_t Out_Size = 0;

    // Size computing - Track part
    uint64_t Track_Size = 0;
    uint64_t LibraryName_Size;
    uint64_t LibraryVersion_Size;
    if (WriteToDisk_Data->IsFirstFrame)
    {
        LibraryName_Size = strlen(LibraryName);
        Track_Size += Size_EB(Name_RawCooked_LibraryName, LibraryName_Size);
        LibraryVersion_Size = strlen(LibraryVersion);
        Track_Size += Size_EB(Name_RawCooked_LibraryVersion, LibraryVersion_Size);
    }

    // Size computing - Block part
    uint64_t Block_Size = 0;
    uint64_t FileName_Size = strlen(WriteToDisk_Data->FileNameDPX);
    Block_Size += Size_EB(Name_RawCooked_FileName, FileName_Size);
    uint64_t BeforeData_Size = 4 + Before_Size; // uncompressed size then data
    Block_Size += Size_EB(Name_RawCooked_BeforeData, BeforeData_Size);
    uint64_t AfterData_Size = 4 + After_Size; // uncompressed size then data
    Block_Size += Size_EB(Name_RawCooked_AfterData, AfterData_Size);

    // Case if the file is unique
    if (Unique)
    {
        Track_Size += Block_Size;
        Block_Size = 0;
    }

    // Size computing - Headers
    if (Track_Size)
        Out_Size += Size_EB(Name_RawCookedTrack, Track_Size);
    if (Block_Size)
        Out_Size += Size_EB(Name_RawCookedBlock, Block_Size);

    // Fill
    uint8_t* Out = new uint8_t[Out_Size];
    size_t Out_Offset = 0;

    // Fill - Track part
    if (Track_Size)
    {
        Put_EB(Out, Out_Offset, Name_RawCookedTrack, Track_Size);
        Put_EB(Out, Out_Offset, Name_RawCooked_LibraryName, LibraryName_Size);
        memcpy(Out + Out_Offset, LibraryName, LibraryName_Size);
        Out_Offset += LibraryName_Size;
        Put_EB(Out, Out_Offset, Name_RawCooked_LibraryVersion, LibraryVersion_Size);
        memcpy(Out + Out_Offset, LibraryVersion, LibraryVersion_Size);
        Out_Offset += LibraryVersion_Size;
    }

    // Fill - Block part
    if (Block_Size)
        Put_EB(Out, Out_Offset, Name_RawCookedBlock, Block_Size);
    Put_EB(Out, Out_Offset, Name_RawCooked_FileName, FileName_Size);
    memcpy(Out + Out_Offset, WriteToDisk_Data->FileNameDPX, FileName_Size);
    Out_Offset += FileName_Size;
    if (BeforeData_Size)
    {
        Put_EB(Out, Out_Offset, Name_RawCooked_BeforeData, BeforeData_Size);
        memset(Out + Out_Offset, 0, 4);
        Out_Offset += 4;
        memcpy(Out + Out_Offset, Before, Before_Size);
        Out_Offset += Before_Size;
    }
    if (AfterData_Size)
    {
        Put_EB(Out, Out_Offset, Name_RawCooked_AfterData, AfterData_Size);
        memset(Out + Out_Offset, 0, 4);
        Out_Offset += 4;
        memcpy(Out + Out_Offset, After, After_Size);
        Out_Offset += After_Size;
    }

    // Write
    if (WriteFileCall)
        WriteFileCall(Out, Out_Size, WriteFileCall_Opaque);
}
