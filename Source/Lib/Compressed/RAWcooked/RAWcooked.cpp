/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
#include "Lib/Config.h"
#include "zlib.h"
#include <algorithm>
#include <cstring>
using namespace std;
//---------------------------------------------------------------------------

// Library name and version
const char* LibraryName = "RAWcooked";
const char* LibraryVersion = "18.10.1";

// EBML
static const uint32_t Name_EBML = 0x0A45DFA3;
static const uint32_t Name_EBML_Doctype = 0x0282;
static const uint32_t Name_EBML_DoctypeVersion = 0x0287;
static const uint32_t Name_EBML_DoctypeReadVersion = 0x0285;

// Top level
static const uint32_t Name_RawCookedSegment = 0x7273;    // "rs", RAWcooked Segment part
static const uint32_t Name_RawCookedAttachment = 0x7261; // "ra" RAWcooked Attachment part
static const uint32_t Name_RawCookedTrack = 0x7274;      // "rt", RAWcooked Track part
static const uint32_t Name_RawCookedBlock = 0x7262;      // "rb", RAWcooked BlockGroup part

// File data
static const uint32_t Name_RawCooked_BeforeData = 0x01;
static const uint32_t Name_RawCooked_AfterData = 0x02;
static const uint32_t Name_RawCooked_MaskBaseBeforeData = 0x03;     // In BlockGroup only
static const uint32_t Name_RawCooked_MaskBaseAfterData = 0x04;      // In BlockGroup only
static const uint32_t Name_RawCooked_MaskAdditionBeforeData = 0x03; // In Track only
static const uint32_t Name_RawCooked_MaskAdditionAfterData = 0x04;  // In Track only
static const uint32_t Name_RawCooked_InData = 0x05;
static const uint32_t Name_RawCooked_MaskBaseInData = 0x06;         // In BlockGroup only
static const uint32_t Name_RawCooked_MaskAdditionInData = 0x06;     // In Track only
// File text metadata
static const uint32_t Name_RawCooked_FileName = 0x10;
static const uint32_t Name_RawCooked_MaskBaseFileName = 0x11;     // In BlockGroup only
static const uint32_t Name_RawCooked_MaskAdditionFileName = 0x11; // In Track only
// File stats
static const uint32_t Name_RawCooked_FileHash = 0x20;
// File number metadata
static const uint32_t Name_RawCooked_FileSize = 0x30;
static const uint32_t Name_RawCooked_MaskBaseFileSize = 0x31;     // In BlockGroup only
static const uint32_t Name_RawCooked_MaskAdditionFileSize = 0x31; // In Track only
// Global information
static const uint32_t Name_RawCooked_LibraryName = 0x70;
static const uint32_t Name_RawCooked_LibraryVersion = 0x71;
static const uint32_t Name_RawCooked_PathSeparator = 0x72;
// Parameters
static const char* DocType = "rawcooked";
static const uint8_t DocTypeVersion = 1;
static const uint8_t DocTypeReadVersion = 1;

//---------------------------------------------------------------------------
enum hashformat
{
    HashFormat_MD5,
};

//---------------------------------------------------------------------------
static size_t Size_EB(uint64_t Value)
{
    size_t S_l = 1;
    while (Value >> (S_l * 7))
        S_l++;
    if (Value == (uint64_t)-1 >> ((sizeof(Value) - S_l) * 8 + S_l))
        S_l++;
    return S_l;
}

//---------------------------------------------------------------------------
static size_t Size_Number(uint64_t Value)
{
    size_t S_l = 1;
    while (Value >> (S_l * 8))
        S_l++;
    return S_l;
}

//---------------------------------------------------------------------------
class ebml_writer
{
public:
    // Constructor / Destructor
    ~ebml_writer();

    // Types
    void EB(uint64_t Size);
    void Block(uint32_t Name, uint64_t Size, const uint8_t* Data);
    void String(uint32_t Name, const char* Data);
    void Number(uint32_t Name, uint64_t Number);
    void DataWithEncodedPrefix(uint32_t Name, uint64_t Prefix, uint64_t Size, const uint8_t* Data);
    void CompressableData(uint32_t Name, const uint8_t* Data, uint64_t Size, uint64_t CompressedSize);

    // Blocks
    void Block_Begin(uint32_t Name);
    void Block_End();
    
    // Buffer
    void Set2ndPass();
    uint8_t* GetBuffer();
    size_t GetBufferSize();

private:
    std::vector<size_t> BlockSizes;
    uint8_t* Buffer = NULL;
    size_t Buffer_PreviousOffset = 0;
    size_t Buffer_Offset = 0;
};

//---------------------------------------------------------------------------
void ebml_writer::EB(uint64_t Size)
{
    size_t S_l = Size_EB(Size);

    if (Buffer)
    {
        uint64_t S2 = Size | (((uint64_t)1) << (S_l * 7));
        while (S_l)
        {
            Buffer[Buffer_Offset] = (uint8_t)(S2 >> ((S_l - 1) * 8));
            Buffer_Offset++;
            S_l--;
        }

    }
    else
        Buffer_Offset += S_l;
}

//---------------------------------------------------------------------------
void ebml_writer::Block(uint32_t Name, uint64_t Size, const uint8_t* Data)
{
    EB(Name);
    EB(Size);

    if (Buffer)
    {
        memcpy(Buffer + Buffer_Offset, Data, Size);
    }
    Buffer_Offset += Size;
}

//---------------------------------------------------------------------------
void ebml_writer::String(uint32_t Name, const char* Data)
{
    Block(Name, strlen(Data), reinterpret_cast<const uint8_t*>(Data));
}

//---------------------------------------------------------------------------
void ebml_writer::Number(uint32_t Name, uint64_t Number)
{
    EB(Name);
    size_t S_l = Size_Number(Number);
    EB(S_l);

    if (Buffer)
    {
        while (S_l)
        {
            Buffer[Buffer_Offset] = (uint8_t)(Number >> ((S_l - 1) * 8));
            Buffer_Offset++;
            S_l--;
        }
    }
    else
        Buffer_Offset += S_l;
}

//---------------------------------------------------------------------------
void ebml_writer::DataWithEncodedPrefix(uint32_t Name, uint64_t Prefix, uint64_t Size, const uint8_t* Data)
{
    EB(Name);
    EB(Size_EB(Prefix) + Size);
    EB(Prefix);

    if (Buffer)
    {
        memcpy(Buffer + Buffer_Offset, Data, Size);
    }
    Buffer_Offset += Size;
}

//---------------------------------------------------------------------------
void ebml_writer::CompressableData(uint32_t Name, const uint8_t* Data, uint64_t Size, uint64_t CompressedSize)
{
    if (!Size)
        return;
    if (CompressedSize)
        DataWithEncodedPrefix(Name, Size, CompressedSize, Data);
    else
        DataWithEncodedPrefix(Name,    0,           Size, Data);
}

//---------------------------------------------------------------------------
void ebml_writer::Block_Begin(uint32_t Name)
{
    EB(Name);
    if (Buffer)
    {
        EB(BlockSizes.back());
        BlockSizes.pop_back();
    }
    else
        Buffer_PreviousOffset = Buffer_Offset;
}

//---------------------------------------------------------------------------
void ebml_writer::Block_End()
{
    if (!Buffer)
    {
        BlockSizes.push_back(Buffer_Offset - Buffer_PreviousOffset);
        EB(Buffer_Offset - Buffer_PreviousOffset);
    }
}

//---------------------------------------------------------------------------
void ebml_writer::Set2ndPass()
{
    if (Buffer)
        return;

    reverse(BlockSizes.begin(), BlockSizes.end()); // We'll decrement size for keeping track about where we are
    Buffer = new uint8_t[Buffer_Offset];
    Buffer_PreviousOffset = 0;
    Buffer_Offset = 0;
}

//---------------------------------------------------------------------------
uint8_t* ebml_writer::GetBuffer()
{
    return Buffer;
}

//---------------------------------------------------------------------------
size_t ebml_writer::GetBufferSize()
{
    return Buffer_Offset;
}

//---------------------------------------------------------------------------
ebml_writer::~ebml_writer()
{
    delete[] Buffer;
}

//---------------------------------------------------------------------------
rawcooked::rawcooked() :
    Unique(false),
    InData(NULL),
    InData_Size(0),
    FileSize((uint64_t)-1),
    BlockCount(0)
{
}

//---------------------------------------------------------------------------
rawcooked::~rawcooked()
{
    Close();
}

//---------------------------------------------------------------------------
void rawcooked::Parse()
{
    // Cross-platform support
    // RAWcooked file format supports setting of the path separator but
    // we currently set all to "/", which is supported by both Windows and Unix based platforms
    // On Windows, we convert "\" to "/" as both are considered as path separators and Unix-based systems don't consider "\" as a path separator
    // If not doing this, files are not considered as in a sub-directory when encoded with a Windows platform then decoded with a Unix-based platform.
    // FormatPath(OutputFileName); // Already done elsewhere

    // Create or Use mask
    uint8_t* FileName = (uint8_t*)OutputFileName.c_str();
    size_t FileName_Size = OutputFileName.size();
    uint8_t* ToStore_FileName = FileName;
    const uint8_t* ToStore_Before = BeforeData;
    const uint8_t* ToStore_After = AfterData;
    const uint8_t* ToStore_In = InData;
    bool IsUsingMask_FileName = false;
    bool IsUsingMask_BeforeData = false;
    bool IsUsingMask_AfterData = false;
    if (!Unique)
    {
        if (!BlockCount)
        {
            if (FileName && FileName_Size)
            {
                FirstFrame_FileName = new uint8_t[FileName_Size];
                memcpy(FirstFrame_FileName, FileName, FileName_Size);
                FirstFrame_FileName_Size = FileName_Size;
            }
            else
            {
                FirstFrame_FileName = NULL;
                FirstFrame_FileName_Size = 0;
            }
            if (BeforeData && BeforeData_Size)
            {
                FirstFrame_Before = new uint8_t[BeforeData_Size];
                memcpy(FirstFrame_Before, BeforeData, BeforeData_Size);
                FirstFrame_Before_Size = BeforeData_Size;
            }
            else
            {
                FirstFrame_Before = NULL;
                FirstFrame_Before_Size = 0;
            }
            if (AfterData && AfterData_Size)
            {
                FirstFrame_After = new uint8_t[AfterData_Size];
                memcpy(FirstFrame_After, AfterData, AfterData_Size);
                FirstFrame_After_Size = AfterData_Size;
            }
            else
            {
                FirstFrame_After = NULL;
                FirstFrame_After_Size = 0;
            }
        }
        if (FileName && FirstFrame_FileName)
        {
            uint8_t* Temp = new uint8_t[FileName_Size];
            memcpy(Temp, FileName, FileName_Size);
            for (size_t i = 0; i < FileName_Size && i < FirstFrame_FileName_Size; i++)
                Temp[i] -= FirstFrame_FileName[i];
            ToStore_FileName = Temp;
            IsUsingMask_FileName = true;
        }
        if (BeforeData && FirstFrame_Before)
        {
            uint8_t* Temp = new uint8_t[BeforeData_Size];
            memcpy(Temp, BeforeData, BeforeData_Size);
            for (size_t i = 0; i < BeforeData_Size && i < FirstFrame_Before_Size; i++)
                Temp[i] -= FirstFrame_Before[i];
            ToStore_Before = Temp;
            IsUsingMask_BeforeData = true;
        }
        if (AfterData && FirstFrame_After)
        {
            uint8_t* Temp = new uint8_t[AfterData_Size];
            memcpy(Temp, AfterData, AfterData_Size);
            for (size_t i = 0; i < AfterData_Size && i < FirstFrame_After_Size; i++)
                Temp[i] -= FirstFrame_After[i];
            ToStore_After = Temp;
            IsUsingMask_AfterData = true;
        }
    }

    // Test if it can be compressed
    uint8_t* Compressed_MaskFileName = NULL;
    uLongf Compressed_MaskFileName_Size = 0;
    if (!Unique && FirstFrame_FileName)
    {
        Compressed_MaskFileName = new uint8_t[FirstFrame_FileName_Size];
        Compressed_MaskFileName_Size = (uLongf)FirstFrame_FileName_Size;
        if (compress((Bytef*)Compressed_MaskFileName, &Compressed_MaskFileName_Size, (const Bytef*)FirstFrame_FileName, (uLong)FirstFrame_FileName_Size) < 0)
        {
            Compressed_MaskFileName = FirstFrame_FileName;
            Compressed_MaskFileName_Size = 0;
        }
    }
    uint8_t* Compressed_FileName = new uint8_t[FileName_Size];
    uLongf Compressed_FileName_Size = (uLongf)FileName_Size;
    if (compress((Bytef*)Compressed_FileName, &Compressed_FileName_Size, (const Bytef*)ToStore_FileName, (uLong)FileName_Size) < 0)
    {
        Compressed_FileName = ToStore_FileName;
        Compressed_FileName_Size = 0;
    }
    uint8_t* Compressed_MaskBeforeData = NULL;
    uLongf Compressed_MaskBeforeData_Size = 0;
    if (!Unique && FirstFrame_Before)
    {
        Compressed_MaskBeforeData = new uint8_t[FirstFrame_Before_Size];
        Compressed_MaskBeforeData_Size = (uLongf)FirstFrame_Before_Size;
        if (compress((Bytef*)Compressed_MaskBeforeData, &Compressed_MaskBeforeData_Size, (const Bytef*)FirstFrame_Before, (uLong)FirstFrame_Before_Size) < 0)
        {
            Compressed_MaskBeforeData = FirstFrame_Before;
            Compressed_MaskBeforeData_Size = 0;
        }
    }
    const uint8_t* Compressed_BeforeData = new uint8_t[BeforeData_Size];
    uLongf Compressed_BeforeData_Size = (uLongf)BeforeData_Size;
    if (compress((Bytef*)Compressed_BeforeData, &Compressed_BeforeData_Size, (const Bytef*)ToStore_Before, (uLong)BeforeData_Size) < 0)
    {
        Compressed_BeforeData = ToStore_Before;
        Compressed_BeforeData_Size = 0;
    }
    const uint8_t* Compressed_MaskAfterData = NULL;
    uLongf Compressed_MaskAfterData_Size = 0;
    if (!Unique && FirstFrame_After)
    {
        Compressed_MaskAfterData = new uint8_t[FirstFrame_After_Size];
        Compressed_MaskAfterData_Size = (uLongf)FirstFrame_After_Size;
        if (compress((Bytef*)Compressed_MaskAfterData, &Compressed_MaskAfterData_Size, (const Bytef*)FirstFrame_After, (uLong)FirstFrame_After_Size) < 0)
        {
            Compressed_MaskAfterData = FirstFrame_After;
            Compressed_MaskAfterData_Size = 0;
        }
    }
    const uint8_t* Compressed_AfterData = new uint8_t[AfterData_Size];
    uLongf Compressed_AfterData_Size = (uLongf)AfterData_Size;
    if (compress((Bytef*)Compressed_AfterData, &Compressed_AfterData_Size, (const Bytef*)ToStore_After, (uLong)AfterData_Size) < 0)
    {
        Compressed_AfterData = ToStore_After;
        Compressed_AfterData_Size = 0;
    }
    const uint8_t* Compressed_InData = new uint8_t[InData_Size];
    uLongf Compressed_InData_Size = (uLongf)InData_Size;
    if (compress((Bytef*)Compressed_InData, &Compressed_InData_Size, (const Bytef*)ToStore_In, (uLong)InData_Size) < 0)
    {
        Compressed_InData = ToStore_In;
        Compressed_InData_Size = 0;
    }

    ebml_writer Writer;
    for (uint8_t Pass = 0; Pass < 2; Pass++)
    {
        // EBML header
        if (!File.IsOpen())
        {
            Writer.Block_Begin(Name_EBML);
            Writer.String(Name_EBML_Doctype, DocType);
            Writer.Number(Name_EBML_DoctypeVersion, 1);
            Writer.Number(Name_EBML_DoctypeReadVersion, 1);
            Writer.Block_End();
        }

        // Segment
        if (!File.IsOpen())
        {
            Writer.Block_Begin(Name_RawCookedSegment);
            Writer.String(Name_RawCooked_LibraryName, LibraryName);
            Writer.String(Name_RawCooked_LibraryVersion, LibraryVersion);
            Writer.Block_End();
        }

        // Track (or attachment) only
        if (!BlockCount)
        {
            Writer.Block_Begin(IsAttachment ? Name_RawCookedAttachment : Name_RawCookedTrack);
            if (!Unique && FirstFrame_FileName)
                Writer.CompressableData(Name_RawCooked_MaskBaseFileName, Compressed_MaskFileName, FirstFrame_FileName_Size, Compressed_MaskFileName_Size);
            if (!Unique && FirstFrame_Before)
                Writer.CompressableData(Name_RawCooked_MaskBaseBeforeData, Compressed_MaskBeforeData, FirstFrame_Before_Size, Compressed_MaskBeforeData_Size);
            if (!Unique && FirstFrame_After)
                Writer.CompressableData(Name_RawCooked_MaskBaseAfterData, Compressed_MaskAfterData, FirstFrame_After_Size, Compressed_MaskAfterData_Size);
            if (!Unique)
                Writer.Block_End();
        }

        // Block only
        if (BlockCount || !Unique)
            Writer.Block_Begin(Name_RawCookedBlock);

        // Common to track and block parts
        Writer.CompressableData(IsUsingMask_FileName ? Name_RawCooked_MaskAdditionFileName : Name_RawCooked_FileName, Compressed_FileName, FileName_Size, Compressed_FileName_Size);
        Writer.CompressableData(IsUsingMask_BeforeData ? Name_RawCooked_MaskAdditionBeforeData : Name_RawCooked_BeforeData, Compressed_BeforeData, BeforeData_Size, Compressed_BeforeData_Size);
        Writer.CompressableData(IsUsingMask_AfterData ? Name_RawCooked_MaskAdditionAfterData : Name_RawCooked_AfterData, Compressed_AfterData, AfterData_Size, Compressed_AfterData_Size);
        Writer.CompressableData(Name_RawCooked_InData, Compressed_InData, InData_Size, Compressed_InData_Size);
        if (HashValue)
            Writer.DataWithEncodedPrefix(Name_RawCooked_FileHash, HashFormat_MD5, HashValue->size(), HashValue->data());
        if (FileSize != (uint64_t)-1)
            Writer.Number(Name_RawCooked_FileSize, FileSize);
        Writer.Block_End();

        // Init 2nd pass
        Writer.Set2ndPass();
    }

    // Write
    WriteToDisk(Writer.GetBuffer(), Writer.GetBufferSize());
    BlockCount++;

    // Clean up
    if (Compressed_FileName_Size)
        delete[] Compressed_FileName;
    if (Compressed_BeforeData_Size)
        delete[] Compressed_BeforeData;
    if (Compressed_AfterData_Size)
        delete[] Compressed_AfterData;
    if (Compressed_InData_Size)
        delete[] Compressed_InData;
    if (IsUsingMask_FileName)
        delete[] ToStore_FileName;
    if (IsUsingMask_BeforeData)
        delete[] ToStore_Before;
    if (IsUsingMask_AfterData)
        delete[] ToStore_After;
}

//---------------------------------------------------------------------------
void rawcooked::ResetTrack()
{
    BlockCount = 0;
}
