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
class masked_buffer : public buffer_base
{
public:
    masked_buffer() {}
    masked_buffer(const buffer_base& Content, const buffer_base& Mask = buffer()) { if (Content) Assign(Content, Mask); }
    ~masked_buffer();

    void                        Assign(const buffer_base& Content, const buffer_base& Mask = buffer());

    bool                        IsUsingMask() const;

private:
    bool                        IsUsingMask_ = false;
};

//---------------------------------------------------------------------------
masked_buffer::~masked_buffer()
{
    if (IsUsingMask_)
        delete[] Data();
}

//---------------------------------------------------------------------------
void masked_buffer::Assign(const buffer_base& Content, const buffer_base& Mask)
{

    if (!Mask)
    {
        AssignBase(Content);
        IsUsingMask_ = false;
        return;
    }

    auto Size = Content.Size();
    uint8_t* Data = new uint8_t[Size];
    memcpy(Data, Content.Data(), Size);
    for (size_t i = 0; i < Size && i < Mask.Size(); i++)
        Data[i] -= Mask[i];
    AssignBase(Data, Size);
    IsUsingMask_ = true;
}

//---------------------------------------------------------------------------
bool masked_buffer::IsUsingMask() const
{
    return IsUsingMask_;
}

//---------------------------------------------------------------------------
class compressed_buffer : public buffer_base
{
public:
    compressed_buffer() : buffer_base() {}
    compressed_buffer(const masked_buffer& Buffer) : buffer_base() { if (Buffer) Assign(Buffer); }
    ~compressed_buffer();

    void                        Assign(const masked_buffer& Buffer);

    size_t                      UncompressedSize() const;

private:
    size_t                      UncompressedSize_ = 0;
};

//---------------------------------------------------------------------------
compressed_buffer::~compressed_buffer()
{
    if (UncompressedSize_)
        delete[] Data();
}

//---------------------------------------------------------------------------
void compressed_buffer::Assign(const masked_buffer& Buffer)
{
    if (!Buffer)
    {
        ClearBase();
        return;
    }

    auto Data = new uint8_t[Buffer.Size()];
    uLongf destLen = (uLongf)Buffer.Size();
    if (compress((Bytef*)Data, &destLen, (const Bytef*)Buffer.Data(), (uLong)Buffer.Size()) < 0)
    {
        // Uncompressed
        delete[] Data;
        AssignBase(Buffer);
        UncompressedSize_ = 0;
        return;
    }

    // Compressed
    AssignBase(Data, destLen);
    UncompressedSize_ = Buffer.Size();
}

//---------------------------------------------------------------------------
size_t compressed_buffer::UncompressedSize() const
{
    return UncompressedSize_;
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
    void DataWithEncodedPrefix(uint32_t Name, uint64_t Prefix, const buffer_base& Data);
    void CompressableData(uint32_t Name, const compressed_buffer& Buffer);

    // Blocks
    void Block_Begin(uint32_t Name);
    void Block_End();
    
    // Buffer
    void Set2ndPass();
    uint8_t* GetBuffer();
    size_t GetBufferSize();

private:
    std::vector<size_t> BlockSizes;
    uint8_t* Buffer = nullptr;
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
void ebml_writer::DataWithEncodedPrefix(uint32_t Name, uint64_t Prefix, const buffer_base& Data)
{
    EB(Name);
    EB(Size_EB(Prefix) + Data.Size());
    EB(Prefix);

    if (Buffer)
    {
        memcpy(Buffer + Buffer_Offset, Data.Data(), Data.Size());
    }
    Buffer_Offset += Data.Size();
}

//---------------------------------------------------------------------------
void ebml_writer::CompressableData(uint32_t Name, const compressed_buffer& Data)
{
    if (!Data)
        return;
    DataWithEncodedPrefix(Name, Data.UncompressedSize(), Data);
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

    // FileName
    auto FileNameData = (const uint8_t*)OutputFileName.c_str();
    auto FileNameData_Size = OutputFileName.size();

    // Create mask when needed
    if (!Unique && !BlockCount)
    {
        FirstFrame_FileName.Create(FileNameData, FileNameData_Size);
        FirstFrame_Before.Create(BeforeData, BeforeData_Size);
        FirstFrame_After.Create(AfterData, AfterData_Size);
    }

    // Apply mask when useful
    masked_buffer Masked_MaskFileName(Unique ? buffer_view() : buffer_view(FirstFrame_FileName));
    masked_buffer Masked_MaskBefore(Unique ? buffer_view() : buffer_view(FirstFrame_Before));
    masked_buffer Masked_MaskAfter(Unique ? buffer_view() : buffer_view(FirstFrame_After));
    masked_buffer Masked_FileName(buffer_view(FileNameData, FileNameData_Size), Unique ? buffer_view() : buffer_view(FirstFrame_FileName));
    masked_buffer Masked_Before(buffer_view(BeforeData, BeforeData_Size), Unique ? buffer_view() : buffer_view(FirstFrame_Before));
    masked_buffer Masked_After(buffer_view(AfterData, AfterData_Size), Unique ? buffer_view() : buffer_view(FirstFrame_After));
    masked_buffer Masked_In(buffer_view(InData, InData_Size));

    // Test if it can be compressed
    compressed_buffer Compressed_MaskFileName(Masked_MaskFileName);
    compressed_buffer Compressed_MaskBefore(Masked_MaskBefore);
    compressed_buffer Compressed_MaskAfter(Masked_MaskAfter);
    compressed_buffer Compressed_FileName(Masked_FileName);
    compressed_buffer Compressed_Before(Masked_Before);
    compressed_buffer Compressed_After(Masked_After);
    compressed_buffer Compressed_In(Masked_In);

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
                Writer.CompressableData(Name_RawCooked_MaskBaseFileName, Compressed_MaskFileName);
            if (!Unique && FirstFrame_Before)
                Writer.CompressableData(Name_RawCooked_MaskBaseBeforeData, Compressed_MaskBefore);
            if (!Unique && FirstFrame_After)
                Writer.CompressableData(Name_RawCooked_MaskBaseAfterData, Compressed_MaskAfter);
            if (!Unique)
                Writer.Block_End();
        }

        // Block only
        if (BlockCount || !Unique)
            Writer.Block_Begin(Name_RawCookedBlock);

        // Common to track and block parts
        Writer.CompressableData(Masked_FileName.IsUsingMask() ? Name_RawCooked_MaskAdditionFileName : Name_RawCooked_FileName, Compressed_FileName);
        Writer.CompressableData(Masked_Before.IsUsingMask() ? Name_RawCooked_MaskAdditionBeforeData : Name_RawCooked_BeforeData, Compressed_Before);
        Writer.CompressableData(Masked_After.IsUsingMask() ? Name_RawCooked_MaskAdditionAfterData : Name_RawCooked_AfterData, Compressed_After);
        Writer.CompressableData(Name_RawCooked_InData, Compressed_In);
        if (HashValue)
            Writer.DataWithEncodedPrefix(Name_RawCooked_FileHash, HashFormat_MD5, buffer_view(HashValue->data(), HashValue->size()));
        if (FileSize != (uint64_t)-1)
            Writer.Number(Name_RawCooked_FileSize, FileSize);
        Writer.Block_End();

        // Init 2nd pass
        Writer.Set2ndPass();
    }

    // Write
    WriteToDisk(Writer.GetBuffer(), Writer.GetBufferSize());
    BlockCount++;
}

//---------------------------------------------------------------------------
void rawcooked::ResetTrack()
{
    BlockCount = 0;
}
