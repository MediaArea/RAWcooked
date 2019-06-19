/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/RAWcooked/RAWcooked.h"
#include "Lib/Config.h"
#include "zlib.h"
#include <algorithm>
#include <cstring>
using namespace std;

//---------------------------------------------------------------------------
// Errors

namespace intermediatewrite_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "cannot create directory",
    "cannot create file",
    "file already exists",
    "cannot write to the file",
    "cannot remove file",
};

enum code : uint8_t
{
    CreateDirectory,
    FileCreate,
    FileAlreadyExists,
    FileWrite,
    FileRemove,
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

} // intermediatewrite_issue

//---------------------------------------------------------------------------
extern void Matroska_CRC32_Compute(uint32_t& CRC32, const uint8_t* Buffer_Current, const uint8_t* Buffer_End);
static void CRC32_Fill(uint8_t* Buffer, size_t Buffer_Size)
{
    // Compute and fill CRC32
    uint32_t CRC32Computed = 0xFFFFFFFF;
    Matroska_CRC32_Compute(CRC32Computed, Buffer, Buffer + Buffer_Size);
    CRC32Computed ^= 0xFFFFFFFF;
    Buffer[-4] = (uint8_t)(CRC32Computed);
    Buffer[-3] = (uint8_t)(CRC32Computed >> 8);
    Buffer[-2] = (uint8_t)(CRC32Computed >> 16);
    Buffer[-1] = (uint8_t)(CRC32Computed >> 24);
}

//---------------------------------------------------------------------------

// Library name and version
const char* LibraryName = "RAWcooked";
const char* LibraryVersion = "18.10.1";

// Global
static const uint32_t CRC32 = 0x3F;
static const uint32_t Void = 0x6C;

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
static const uint32_t Name_RawCooked_MaskAdditionFileSize = 0x32; // In Track only
// Segment information
static const uint32_t Name_RawCooked_LibraryName = 0x70;
static const uint32_t Name_RawCooked_LibraryVersion = 0x71;
static const uint32_t Name_RawCooked_PathSeparator = 0x72;
// Erasure
static const uint32_t Name_Rawcooked_Erasure = 0x726365;   // "rce", RAWcooked global part, EBML class D
static const uint32_t Name_Rawcooked_Erasure_ShardInfo = 0x726369;   // "rci", RAWcooked global part, EBML class D
static const uint32_t Name_Rawcooked_Erasure_ShardHashes = 0x726368;   // "rch", RAWcooked global part, EBML class D
static const uint32_t Name_Rawcooked_Erasure_ParityShards = 0x726370;   // "rcp", RAWcooked global part, EBML class D
static const uint32_t Name_Rawcooked_Erasure_EbmlStartLocation = 0x726373;   // "rcs", RAWcooked global part, EBML class D
static const uint32_t Name_Rawcooked_Erasure_ParityShardsLocation = 0x72636C;   // "rcl", RAWcooked global part, EBML class D
// Parameters
static const char* DocType = "rawcooked";
static const uint64_t DocTypeVersion = 1;
static const uint64_t DocTypeReadVersion = 1;

//---------------------------------------------------------------------------
enum hashformat
{
    HashFormat_MD5,
};

//---------------------------------------------------------------------------
static size_t Size_EB(uint64_t Value)
{
    size_t S_l = 1;
    while (S_l < 7 && Value >> (S_l * 7))
        S_l++;
    if (Value == (uint64_t)-1 >> ((sizeof(Value) - S_l) * 8 + S_l))
        S_l++;
    return S_l;
}

//---------------------------------------------------------------------------
static void Write_EB(uint8_t* Buffer, size_t& Buffer_Offset, uint64_t Value)
{
    size_t S_l = Size_EB(Value);
    uint64_t S2 = Value | (((uint64_t)1) << (S_l * 7));
    while (S_l)
    {
        Buffer[Buffer_Offset] = (uint8_t)(S2 >> ((S_l - 1) * 8));
        Buffer_Offset++;
        S_l--;
    }
}

//---------------------------------------------------------------------------
static size_t Size_Number(uint64_t Value)
{
    size_t S_l = 1;
    while (S_l < 7 && Value >> (S_l * 8))
        S_l++;
    return S_l;
}

//---------------------------------------------------------------------------
static size_t Size_Number(int64_t Value)
{
    size_t S_l = 1;
    while (S_l < 7 && Value >> (S_l * 8) == ((int64_t)-1) >> (S_l * 8))
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
    void EB(uint64_t Value, size_t Length = (size_t)-1);
    void Block(uint32_t Name, uint64_t Size, const uint8_t* Data, size_t BlockSize_Length = (size_t)-1);
    void String(uint32_t Name, const char* Data);
    void Number(uint32_t Name, uint64_t Number, size_t BlockSize_Length = (size_t)-1);
    void Number(uint32_t Name, int64_t Number, size_t BlockSize_Length = (size_t)-1);
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
    struct block_info
    {
        size_t Buffer_PreviousOffset = 0;
        size_t Size = 0;
        std::vector<block_info> Blocks;

        size_t GetSize(size_t Level)
        {
            if (Level)
                return Blocks.back().GetSize(Level - 1);
            else
                return  Blocks.back().Size;
        }

        void Remove(size_t Level)
        {
            if (Level)
                return Blocks.back().Remove(Level - 1);
            else
                Blocks.pop_back();
        }

        void Begin(size_t Offset, size_t Level)
        {
            if (Level)
                return Blocks.back().Begin(Offset, Level - 1);
            else
            {
                Blocks.push_back(block_info());
                Blocks.back().Buffer_PreviousOffset = Offset;
            }
        }

        void End(size_t Offset, size_t Level)
        {
            if (Level)
                return Blocks.back().End(Offset, Level - 1);
            else
            {
                Blocks.back().Size = Offset - Blocks.back().Buffer_PreviousOffset;
            }
        }

        void Set2ndPass()
        {
            Buffer_PreviousOffset = 0;
            reverse(Blocks.begin(), Blocks.end()); // We'll decrement size for keeping track about where we are
            for (auto Block : Blocks)
                Block.Set2ndPass();
        }
    };
    block_info BlockInfo;
    size_t Level = (size_t)-1;
    uint8_t* Buffer = NULL;
    size_t Buffer_Offset = 0;
};

//---------------------------------------------------------------------------
void ebml_writer::EB(uint64_t Value, size_t S_l)
{
    if (S_l > 7)
        S_l = Size_EB(Value);

    if (Buffer)
    {
        uint64_t S2 = Value | (((uint64_t)1) << (S_l * 7));
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
void ebml_writer::Block(uint32_t Name, uint64_t Size, const uint8_t* Data, size_t S_l)
{
    EB(Name);
    EB(Size, S_l);

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
void ebml_writer::Number(uint32_t Name, uint64_t Number, size_t S_l)
{
    EB(Name);
    if (S_l > 7)
        S_l = Size_Number(Number);
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
void ebml_writer::Number(uint32_t Name, int64_t Number, size_t S_l)
{
    EB(Name);
    if (S_l > 7)
        S_l = Size_Number(Number);
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
    Level++;

    EB(Name);
    if (Buffer)
    {
        EB(BlockInfo.GetSize(Level));
    }
    else
    {
        BlockInfo.Begin(Buffer_Offset, Level);
    }
}

//---------------------------------------------------------------------------
void ebml_writer::Block_End()
{
    if (!Buffer)
    {
        BlockInfo.End(Buffer_Offset, Level);
        EB(BlockInfo.GetSize(Level));
    }
    else
    {
        BlockInfo.Remove(Level);
    }

    Level--;
}

//---------------------------------------------------------------------------
void ebml_writer::Set2ndPass()
{
    if (Buffer)
        return;

    BlockInfo.Set2ndPass();
    Buffer = new uint8_t[Buffer_Offset];
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
    delete EbmlWriter;
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
    uint8_t* ToStore_Before = BeforeData;
    uint8_t* ToStore_After = AfterData;
    uint8_t* ToStore_In = InData;
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
            ToStore_FileName = new uint8_t[FileName_Size];
            memcpy(ToStore_FileName, FileName, FileName_Size);
            for (size_t i = 0; i < FileName_Size || i < FirstFrame_FileName_Size; i++)
                ToStore_FileName[i] -= FirstFrame_FileName[i];
            IsUsingMask_FileName = true;
        }
        if (BeforeData && FirstFrame_Before)
        {
            ToStore_Before = new uint8_t[BeforeData_Size];
            memcpy(ToStore_Before, BeforeData, BeforeData_Size);
            for (size_t i = 0; i < BeforeData_Size || i < FirstFrame_Before_Size; i++)
                ToStore_Before[i] -= FirstFrame_Before[i];
            IsUsingMask_BeforeData = true;
        }
        if (AfterData && FirstFrame_After)
        {
            ToStore_After = new uint8_t[AfterData_Size];
            memcpy(ToStore_After, AfterData, AfterData_Size);
            for (size_t i = 0; i < AfterData_Size || i < FirstFrame_After_Size; i++)
                ToStore_After[i] -= FirstFrame_After[i];
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
    uint8_t* Compressed_BeforeData = new uint8_t[BeforeData_Size];
    uLongf Compressed_BeforeData_Size = (uLongf)BeforeData_Size;
    if (compress((Bytef*)Compressed_BeforeData, &Compressed_BeforeData_Size, (const Bytef*)ToStore_Before, (uLong)BeforeData_Size) < 0)
    {
        Compressed_BeforeData = ToStore_Before;
        Compressed_BeforeData_Size = 0;
    }
    uint8_t* Compressed_MaskAfterData = NULL;
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
    uint8_t* Compressed_AfterData = new uint8_t[AfterData_Size];
    uLongf Compressed_AfterData_Size = (uLongf)AfterData_Size;
    if (compress((Bytef*)Compressed_AfterData, &Compressed_AfterData_Size, (const Bytef*)ToStore_After, (uLong)AfterData_Size) < 0)
    {
        Compressed_AfterData = ToStore_After;
        Compressed_AfterData_Size = 0;
    }
    uint8_t* Compressed_InData = new uint8_t[InData_Size];
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
            Writer.Number(Name_EBML_DoctypeVersion, DocTypeVersion);
            Writer.Number(Name_EBML_DoctypeReadVersion, DocTypeReadVersion);
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

//---------------------------------------------------------------------------
void rawcooked::Close()
{
    File.SetEndOfFile();
    File.Close();
}

//---------------------------------------------------------------------------
void rawcooked::Delete()
{
    if (!File_WasCreated)
        return;

    // Delete the temporary file
    int Result = remove(FileName.c_str());
    if (Result)
    {
        if (Errors)
            Errors->Error(IO_IntermediateWriter, error::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileRemove, FileName);
        return;
    }
}

//---------------------------------------------------------------------------
void rawcooked::WriteToDisk(uint8_t* Buffer, size_t Buffer_Size, bool Append)
{
    if (!File_WasCreated)
    {
        bool RejectIfExists = (Append || (Mode && *Mode == AlwaysYes)) ? false : true;
        if (file::return_value Result = File.Open_WriteMode(string(), FileName, RejectIfExists))
        {
            if (RejectIfExists && Result == file::Error_FileAlreadyExists && Mode && *Mode == Ask && Ask_Callback)
            {
                user_mode NewMode = Ask_Callback(Mode, FileName, string(), false, *ProgressIndicator_IsPaused, *ProgressIndicator_IsEnd);
                if (NewMode == AlwaysYes)
                    Result = File.Open_WriteMode(string(), FileName, false);
            }

            if (Result)
            {
                if (Errors)
                    switch (Result)
                    {
                        case file::Error_CreateDirectory:       Errors->Error(IO_IntermediateWriter, error::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::CreateDirectory, FileName); break;
                        case file::Error_FileCreate:            Errors->Error(IO_IntermediateWriter, error::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileCreate, FileName); break;
                        case file::Error_FileAlreadyExists:     Errors->Error(IO_IntermediateWriter, error::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileAlreadyExists, FileName); break;
                        default:                                Errors->Error(IO_IntermediateWriter, error::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileWrite, FileName); break;
                    }
                return;
            }
        }
        if (Append)
        {
            if (File.Seek(0, file::End))
            {
                if (Errors)
                    Errors->Error(IO_IntermediateWriter, error::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileWrite, FileName);
                return;
            }
        }
        File_WasCreated = true;
    }

    if (File.Write(Buffer, Buffer_Size))
        return;
}

//---------------------------------------------------------------------------
void rawcooked::Erasure(hash_value* HashValues, uint8_t* ParityShards, size_t Buffer_Size)
{
    // Create
    size_t DataHashes_Count = (Buffer_Size + 1024 * 1024 - 1) / (1024 * 1024);
    size_t ParityHashes_Count = (DataHashes_Count + 284 - 1) / 248;
    ParityHashes_Count *= 8;

    size_t Offset;
    size_t ShardInfo_Size = Size_EB(248) + Size_EB(8) + Size_EB(1024 * 1024) + Size_EB(0) + Size_EB(Buffer_Size);
    uint8_t* ShardInfo = new uint8_t[ShardInfo_Size];
    Offset = 0;
    Write_EB(ShardInfo, Offset, 248);
    Write_EB(ShardInfo, Offset, 8);
    Write_EB(ShardInfo, Offset, 1024 * 1024);
    Write_EB(ShardInfo, Offset, 0);
    Write_EB(ShardInfo, Offset, Buffer_Size);
    size_t ShardHashes_Size = Size_EB(0) + 16 * (DataHashes_Count + ParityHashes_Count);
    uint8_t* ShardHashes = new uint8_t[ShardHashes_Size];
    Offset = 0;
    Write_EB(ShardHashes, Offset, 0);
    for (size_t i = 0; i < DataHashes_Count; i++)
    {
        memcpy(ShardHashes + Offset, HashValues[i].Values, 16);
        Offset += 16;
    }
    for (size_t i = 0; i < ParityHashes_Count; i++)
    {
        memcpy(ShardHashes + Offset, HashValues[DataHashes_Count + i].Values, 16);
        Offset += 16;
    }

    delete EbmlWriter;
    EbmlWriter = new ebml_writer;
    auto& Writer = *EbmlWriter;
    size_t Padding_Size, Padding_Size2;
    vector<int64_t> Locations;
    for (uint8_t Pass = 0; Pass < 2; Pass++)
    {
        // EBML header
        if (!Pass)
            Locations.push_back(Writer.GetBufferSize());
        Writer.Block_Begin(Name_EBML);
        Writer.String(Name_EBML_Doctype, DocType);
        Writer.Number(Name_EBML_DoctypeVersion, DocTypeVersion);
        Writer.Number(Name_EBML_DoctypeReadVersion, DocTypeReadVersion);
        Writer.Block_End();

        // Segment - before padding
        Writer.Block_Begin(Name_RawCookedSegment);
        Writer.String(Name_RawCooked_LibraryName, LibraryName);
        Writer.String(Name_RawCooked_LibraryVersion, LibraryVersion);
        Writer.Block_Begin(Name_Rawcooked_Erasure);
        Writer.Block(Name_Rawcooked_Erasure_ShardInfo, ShardInfo_Size, ShardInfo);
        Writer.Block(Name_Rawcooked_Erasure_ShardHashes, ShardHashes_Size, ShardHashes);

        // Segment - padding
        if (!Pass)
            Padding_Size = 1024 * 1024 - ((Buffer_Size + Writer.GetBufferSize() + 12 /*TEMP*/ + Size_EB(Name_Rawcooked_Erasure_ParityShards) + Size_EB(ParityHashes_Count * 1024 * 1024)) % (1024 * 1024));
        Writer.Block(Void, Padding_Size, ParityShards, 3); //TEMP ParityShards

        // Segment - after padding
        Writer.Block(Name_Rawcooked_Erasure_ParityShards, ParityHashes_Count * 1024 * 1024, ParityShards);
        Writer.Block_End();
        Writer.Block_End();
        if (!Pass)
            Locations.push_back(Writer.GetBufferSize());

        for (uint8_t i = 0; i < 8 + 1; i++)
        {
            // EBML header
            Writer.Block_Begin(Name_EBML);
            Writer.String(Name_EBML_Doctype, DocType);
            Writer.Number(Name_EBML_DoctypeVersion, DocTypeVersion);
            Writer.Number(Name_EBML_DoctypeReadVersion, DocTypeReadVersion);
            Writer.Block_End();

            // Segment - before padding
            Writer.Block_Begin(Name_RawCookedSegment);
            Writer.String(Name_RawCooked_LibraryName, LibraryName);
            Writer.String(Name_RawCooked_LibraryVersion, LibraryVersion);
            Writer.Block_Begin(Name_Rawcooked_Erasure);
            const uint32_t Zero = 0;
            Writer.Block(CRC32, 4, (const uint8_t*)&Zero);
            size_t CRC32_Start = Writer.GetBufferSize();
            Writer.Block(Name_Rawcooked_Erasure_ShardInfo, ShardInfo_Size, ShardInfo);
            Writer.Block(Name_Rawcooked_Erasure_ShardHashes, ShardHashes_Size, ShardHashes);

            // Segment - padding
            size_t LastPartBlockSize = (Size_EB(Name_Rawcooked_Erasure_EbmlStartLocation) + Size_EB(4) + 4);
            if (!i && !Pass)
                Padding_Size2 = 1024 * 1024 - ((Writer.GetBufferSize() - Locations[1] + 10 /*TEMP*/ + LastPartBlockSize * (1 + 8 + 1 + 1)) % (1024 * 1024));
            Writer.Block(Void, Padding_Size2, ParityShards, 3); //TEMP ParityShards

            // Segment - after padding
            Writer.Number(Name_Rawcooked_Erasure_EbmlStartLocation, Pass ? (Locations[0] - Locations[1 + i + 1] + LastPartBlockSize * (1 + 8 + 1)) : 0, 4);
            for (uint8_t j = 0; j < 8 + 1; j++)
                Writer.Number(Name_Rawcooked_Erasure_EbmlStartLocation, Pass ? (Locations[1 + j] - Locations[1 + i + 1] + LastPartBlockSize * (1 + 8 - j)) : 0, 4);
            Writer.Number(Name_Rawcooked_Erasure_ParityShardsLocation, Pass ? (Locations[1] - ParityHashes_Count * 1024 * 1024 - Locations[1 + i + 1]) : 0, 4);
            Writer.Block_End();
            Writer.Block_End();
            if (!Pass)
                Locations.push_back(Writer.GetBufferSize());
            else
                CRC32_Fill(Writer.GetBuffer() + CRC32_Start, Writer.GetBufferSize() - CRC32_Start); // Compute and fill CRC32
        }

        // Init 2nd pass
        Writer.Set2ndPass();
    }
}

//---------------------------------------------------------------------------
void rawcooked::Erasure_AppendToFile()
{
    if (!EbmlWriter)
        return; // Nothing to do

    // Write
    auto& Writer = *EbmlWriter;
    WriteToDisk(Writer.GetBuffer(), Writer.GetBufferSize(), true);
}
