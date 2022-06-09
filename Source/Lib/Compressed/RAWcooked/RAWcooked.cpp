/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
#include "Lib/Common/Common.h"
#include "zlib.h"
#include <algorithm>
#include <cstring>
using namespace std;
//---------------------------------------------------------------------------

// Library name and version
const char* LibraryName = "RAWcooked";
const char* LibraryVersion = "21.09";

// EBML
static const uint32_t Name_EBML = 0x0A45DFA3;
static const uint32_t Name_EBML_Doctype = 0x0282;
static const uint32_t Name_EBML_DoctypeVersion = 0x0287;
static const uint32_t Name_EBML_DoctypeReadVersion = 0x0285;

// Top level
static const uint32_t Name_RawCooked = 0x7263; // "rc", RAWcooked root
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
class compressed_buffer : public buffer_base
{
public:
    compressed_buffer() : buffer_base() {}
    compressed_buffer(const buffer_base& Content, const buffer_base& Mask = buffer()) : buffer_base() { if (Content) Assign(Content, Mask); }
    ~compressed_buffer();

    void                        Assign(const buffer_base& Content, const buffer_base& Mask);

    bool                        IsUsingMask() const;
    size_t                      UncompressedSize() const;

private:
    buffer                      Masked; // Size is the max size
    buffer                      Compressed; // Size is the max size
    uLongf                      UncompressedSize_ = 0;
    bool                        IsUsingMask_ = false;
};

//---------------------------------------------------------------------------
compressed_buffer::~compressed_buffer()
{
}

//---------------------------------------------------------------------------
void compressed_buffer::Assign(const buffer_base& Content, const buffer_base& Mask)
{
    // If empty
    if (!Content)
    {
        ClearBase();
        UncompressedSize_ = 0;
        IsUsingMask_ = false;
        return;
    }

    // Mask
    auto Content_Size = Content.Size();
    const uint8_t* Temp;
    if (!Mask.Empty())
    {
        if (Content_Size > Masked.Size())
            Masked.Create(Content_Size);
        for (size_t i = 0; i < Content_Size && i < Mask.Size(); i++)
            Masked[i] = Content[i] - Mask[i]; // Inverse apply mask
        if (Content_Size > Mask.Size())
            memcpy(Masked.Data() + Mask.Size(), Content.Data() + Mask.Size(), Content_Size - Mask.Size());
        Temp = Masked.Data();
        IsUsingMask_ = true;
    }
    else
    {
        Temp = Content.Data();
        IsUsingMask_ = false;
    }

    // Compression
    if (Content_Size > (uLongf)-1) // Unlikely
    {
        AssignBase(Content);
        UncompressedSize_ = 0;
        return;
    }
    if (Content_Size > Compressed.Size())
        Compressed.Create(Content_Size);
    UncompressedSize_ = (uLongf)Content_Size;
    auto dest = (Bytef*)Compressed.Data();
    auto destLen = UncompressedSize_;
    auto src = (const Bytef*)Temp;
    auto srcLen = (uLong)Content.Size();
    if (compress2(dest, &destLen, src, srcLen, 1) < 0)
    {
        // Uncompressed
        AssignBase(src, srcLen);
        UncompressedSize_ = 0;
        return;
    }
    AssignBase(dest, destLen); // External size is the compressed size, not the maximal buffer size
    UncompressedSize_ = srcLen;
}

//---------------------------------------------------------------------------
bool compressed_buffer::IsUsingMask() const
{
    return IsUsingMask_;
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
    void BlockHeaderOnly(uint32_t Name, size_t Size);
    void String(uint32_t Name, const char* Data);
    void Number(uint32_t Name, uint64_t Number);
    void DataWithEncodedPrefix(uint32_t Name, uint64_t Prefix, const buffer_base& Data);
    void CompressableData(uint32_t Name, const compressed_buffer& Buffer);

    // Blocks
    void Block_Begin(uint32_t Name);
    void Block_End(size_t Extra = 0);

    // Buffer
    void Set1stPass();
    void Set2ndPass();
    uint8_t* GetBuffer();
    size_t GetBufferSize();

private:
    struct block
    {
        size_t HeaderNameSize = 0;
        size_t HeaderSizeSize = 0;
        size_t ContentSize = 0;
        std::vector<block> Blocks;
        size_t GetSize(bool Full = false) const
        {
            size_t Temp = +ContentSize;
            if (Full)
                Temp += HeaderNameSize + HeaderSizeSize;
            for (const auto& Block : Blocks)
                Temp += Block.GetSize(true);
            return Temp;
        }
        void Reverse()
        {
            for (auto& Block : Blocks)
                Block.Reverse();
            reverse(Blocks.begin(), Blocks.end()); // We'll decrement size for keeping track about where we are
        }
    };
    std::vector<block> Blocks;
    size_t Blocks_Level = 0;
    std::vector<block>* Blocks_Current()
    {
        std::vector<block>* Current = &Blocks;
        for (size_t i = 1; i < Blocks_Level; i++)
        {
            Current = &(*Current)[Current->size() - 1].Blocks;
        }
        return Current;
    }
    void Blocks_New()
    {
        auto Current = Blocks_Current();
        Current->resize(Current->size() + 1);
    }
    uint8_t* Buffer = nullptr;
    size_t Buffer_PreviousOffset = 0;
    size_t Buffer_Offset = 0;
    size_t Buffer_MaxSize = 0;
    bool   IsSecondPass_ = false;
};

//---------------------------------------------------------------------------
void ebml_writer::EB(uint64_t Size)
{
    size_t S_l = Size_EB(Size);

    if (IsSecondPass_)
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

    if (IsSecondPass_)
    {
        memcpy(Buffer + Buffer_Offset, Data, Size);
    }
    Buffer_Offset += Size;
}

//---------------------------------------------------------------------------
void ebml_writer::BlockHeaderOnly(uint32_t Name, size_t Size)
{
    EB(Name);
    EB(Size);
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

    if (IsSecondPass_)
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

    if (IsSecondPass_)
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
    size_t Temp = Buffer_Offset;
    EB(Name);
    if (IsSecondPass_)
    {
        EB(Blocks_Current()->back().GetSize());
        if (Blocks_Current()->back().Blocks.empty())
        {
            Blocks_Current()->pop_back();
            auto Blocks_Level_Save = Blocks_Level;
            while (Blocks_Level)
            {
                Blocks_Level--;
                if (!Blocks_Current()->back().Blocks.empty())
                    break;
                Blocks_Current()->pop_back();
            }
            Blocks_Level = Blocks_Level_Save;
        }
    }
    else
    {
        Buffer_PreviousOffset = Buffer_Offset;
        Blocks_New();
        Blocks_Current()->back().HeaderNameSize = Buffer_Offset - Temp;
    }
    Blocks_Level++;
}

//---------------------------------------------------------------------------
void ebml_writer::Block_End(size_t Extra)
{
    if (IsSecondPass_)
    {
    }
    else
    {
        if (Blocks_Current()->back().Blocks.empty())
            Blocks_Current()->back().ContentSize = Buffer_Offset - Buffer_PreviousOffset + Extra;
        size_t Temp = Buffer_Offset;
        EB(Blocks_Current()->back().GetSize(true) + Extra);
        Blocks_Current()->back().HeaderSizeSize += Buffer_Offset - Temp;
    }
    Blocks_Level--;
}

//---------------------------------------------------------------------------
void ebml_writer::Set1stPass()
{
    Blocks.clear();
    Blocks_Level = 0;
    Buffer_PreviousOffset = 0;
    Buffer_Offset = 0;
    IsSecondPass_ = false;
}

//---------------------------------------------------------------------------
void ebml_writer::Set2ndPass()
{
    for (auto& Block : Blocks)
        Block.Reverse();
    reverse(Blocks.begin(), Blocks.end()); // We'll decrement size for keeping track about where we are
    if (Buffer_Offset > Buffer_MaxSize)
    {
        delete[] Buffer;
        Buffer = new uint8_t[Buffer_Offset];
        Buffer_MaxSize = Buffer_Offset;
    }
    Buffer_PreviousOffset = 0;
    Buffer_Offset = 0;
    IsSecondPass_ = true;
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
ENUM_BEGIN(element)
MaskFileName,
MaskBefore,
MaskAfter,
FileName,
Before,
After,
In,
ENUM_END(element)

//---------------------------------------------------------------------------
class rawcooked::private_data
{
public:
    // File IO
    size_t                      BlockCount = 0;

    // First frame info
    buffer                      FirstFrame[3];

    // Analysis
    void                        Parse(element Element, const buffer_base& Content, const buffer_base& Mask = buffer());
    const compressed_buffer& Compressed(element Element);
    bool                        IsUsingMask(element Element);
    long long                   TotalSize = 0;

    // Write
    ebml_writer                 Writer;

private:
    compressed_buffer           Buffers[element_Max];
};

//---------------------------------------------------------------------------
void rawcooked::private_data::Parse(element Element, const buffer_base& Content, const buffer_base& Mask)
{
    auto& Buffer = Buffers[(size_t)Element];
    Buffer.Assign(Content, Mask);
}

//---------------------------------------------------------------------------
const compressed_buffer& rawcooked::private_data::Compressed(element Element)
{
    return Buffers[(size_t)Element];
}

//---------------------------------------------------------------------------
bool rawcooked::private_data::IsUsingMask(element Element)
{
    return Buffers[(size_t)Element].IsUsingMask();
}

//---------------------------------------------------------------------------
rawcooked::rawcooked() :
    Data_(new private_data)
{
}

//---------------------------------------------------------------------------
rawcooked::~rawcooked()
{
    Close();
    delete Data_;
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

    // Temp
    auto& BlockCount = Data_->BlockCount;
    auto& FirstFrame = Data_->FirstFrame;

    // Create mask when needed
    if (!Unique && !BlockCount)
    {
        FirstFrame[(size_t)element::MaskFileName].Create(FileNameData, FileNameData_Size);
        FirstFrame[(size_t)element::MaskBefore].Create(BeforeData, BeforeData_Size);
        FirstFrame[(size_t)element::MaskAfter].Create(AfterData, AfterData_Size);
    }

    // Apply mask and/or compress when useful
    Data_->Parse(element::MaskFileName, Unique ? buffer_view() : buffer_view(FirstFrame[(size_t)element::MaskFileName]));
    Data_->Parse(element::MaskBefore, Unique ? buffer_view() : buffer_view(FirstFrame[(size_t)element::MaskBefore]));
    Data_->Parse(element::MaskAfter, Unique ? buffer_view() : buffer_view(FirstFrame[(size_t)element::MaskAfter]));
    Data_->Parse(element::FileName, buffer_view(FileNameData, FileNameData_Size), Unique ? buffer_view() : buffer_view(FirstFrame[(size_t)element::MaskFileName]));
    Data_->Parse(element::Before, buffer_view(BeforeData, BeforeData_Size), Unique ? buffer_view() : buffer_view(FirstFrame[(size_t)element::MaskBefore]));
    Data_->Parse(element::After, buffer_view(AfterData, AfterData_Size), Unique ? buffer_view() : buffer_view(FirstFrame[(size_t)element::MaskAfter]));
    Data_->Parse(element::In, buffer_view(InData, InData_Size));

    auto& Writer = Data_->Writer;;
    Writer.Set1stPass();
    for (uint8_t Pass = 0; Pass < 2; Pass++)
    {
        // EBML header
        if (!File.IsOpen())
        {
            Writer.Block_Begin(Name_EBML);
            Writer.String(Name_EBML_Doctype, DocType);
            Writer.Number(Name_EBML_DoctypeVersion, ReversibilityFile ? 2 : 1);
            Writer.Number(Name_EBML_DoctypeReadVersion, ReversibilityFile ? 2 : 1);
            Writer.Block_End();
        }

        if (ReversibilityFile)
        {
            auto Start = Writer.GetBufferSize();
            Writer.Block_Begin(Name_RawCooked);

            if (!Pass)
            {
                Writer.Block_End(ReversibilityFile->Size() - Start);

                // Init 2nd pass
                Writer.Set2ndPass();
            }
            else
            {
                // Copy content
                File_WasCreated = true;
                if (auto Result = File.Open_WriteMode(string(), FileName))
                {
                    SetErrorFileBecomingTooBig(); //TODO: dedicated error
                    return;
                }
                if (auto Result = File.Seek(0, file::End))
                {
                    SetErrorFileBecomingTooBig(); //TODO: dedicated error
                    return;
                }
                WriteToDisk(Writer.GetBuffer(), Writer.GetBufferSize());

                size_t CopySize = 1024 * 1024;
                auto End = ReversibilityFile->Size();
                for (size_t Offset = Start; Offset < End; Offset += CopySize)
                {
                    auto CopySize_Max = End - Offset;
                    if (CopySize > CopySize_Max)
                        CopySize = CopySize_Max;
                    WriteToDisk(ReversibilityFile->Data() + Offset, CopySize);
                    ReversibilityFile->Remap();
                }

                return;
            }

            continue;
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
            if (!Unique && FirstFrame[(size_t)element::MaskFileName])
                Writer.CompressableData(Name_RawCooked_MaskBaseFileName, Data_->Compressed(element::MaskFileName));
            if (!Unique && FirstFrame[(size_t)element::MaskBefore])
                Writer.CompressableData(Name_RawCooked_MaskBaseBeforeData, Data_->Compressed(element::MaskBefore));
            if (!Unique && FirstFrame[(size_t)element::MaskAfter])
                Writer.CompressableData(Name_RawCooked_MaskBaseAfterData, Data_->Compressed(element::MaskAfter));
            if (!Unique)
                Writer.Block_End();
        }

        // Block only
        if (BlockCount || !Unique)
            Writer.Block_Begin(Name_RawCookedBlock);

        // Common to track and block parts
        Writer.CompressableData(Data_->IsUsingMask(element::FileName) ? Name_RawCooked_MaskAdditionFileName : Name_RawCooked_FileName, Data_->Compressed(element::FileName));
        Writer.CompressableData(Data_->IsUsingMask(element::Before) ? Name_RawCooked_MaskAdditionBeforeData : Name_RawCooked_BeforeData, Data_->Compressed(element::Before));
        Writer.CompressableData(Data_->IsUsingMask(element::After) ? Name_RawCooked_MaskAdditionAfterData : Name_RawCooked_AfterData, Data_->Compressed(element::After));
        Writer.CompressableData(Name_RawCooked_InData, Data_->Compressed(element::In));
        if (HashValue)
            Writer.DataWithEncodedPrefix(Name_RawCooked_FileHash, HashFormat_MD5, buffer_view(HashValue->data(), HashValue->size()));
        if (FileSize != (uint64_t)-1)
            Writer.Number(Name_RawCooked_FileSize, FileSize);
        Writer.Block_End();

        // Init 2nd pass
        if (!Pass)
            Writer.Set2ndPass();
    }

    // Write
    WriteToDisk(Writer.GetBuffer(), Writer.GetBufferSize());
    Data_->BlockCount++;

    // Handle too big output files
    Data_->TotalSize += Writer.GetBufferSize();
    if (Data_->TotalSize > 0x10000000) // TODO: add an option for avoiding this hard coded value; this value is from FFmpeg (prevent read by FFmpeg, with >= 0x40000000 older FFmpeg create invalid files)
    {
        if (Version == version::mini)
            Version = version::v2;
        if (Version != version::v2)
            SetErrorFileBecomingTooBig();
    }
}

//---------------------------------------------------------------------------
void rawcooked::ResetTrack()
{
    Data_->BlockCount = 0;
}
