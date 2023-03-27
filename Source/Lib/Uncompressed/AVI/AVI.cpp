/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Uncompressed/AVI/AVI.h"
#include "Lib/Uncompressed/WAV/WAV.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace avi_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "file smaller than expected",
    "RIFF chunk size",
    "fmt  chunk size",
    "truncated chunk",
};

enum code : uint8_t
{
    BufferOverflow,
    RIFF_ChunkSize,
    fmt__ChunkSize,
    TruncatedChunk,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unparsable

namespace unsupported
{

static const char* MessageText[] =
{
    // Unsupported
    "RF64 (4GB+ AVI)",
    "fmt FormatTag not PCM 1 or 3",
    "fmt AvgBytesPerSec",
    "fmt BlockAlign",
    "fmt extension",
    "fmt cbSize",
    "fmt ValidBitsPerSample",
    "fmt ChannelMask",
    "fmt SubFormat not KSDATAFORMAT_SUBTYPE_PCM 00000001-0000-0010-8000-00AA00389B71",
    "fmt chunk not before data chunk",
    "Flavor (SamplesPerSec / BitDepth / Channels / Endianness combination)",
    "pixels in slice not on a 4x32-bit boundary",
};

enum code : uint8_t
{
    RF64,
    fmt__FormatTag,
    fmt__AvgBytesPerSec,
    fmt__BlockAlign,
    fmt__extension,
    fmt__cbSize,
    fmt__ValidBitsPerSample,
    fmt__ChannelMask,
    fmt__SubFormat,
    fmt__Location,
    Flavor,
    PixelBoundaries,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unsupported

const char** ErrorTexts[] =
{
    undecodable::MessageText,
    unsupported::MessageText,
    nullptr,
    nullptr,
};

static_assert(error::type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // avi_issue

using namespace avi_issue;

//---------------------------------------------------------------------------
// Tested cases
extern wav::flavor Wav_CheckSupported(uint16_t FormatTag, uint16_t Channels, uint32_t SamplesPerSec, uint16_t BitDepth);

//---------------------------------------------------------------------------


#define ELEMENT_BEGIN(_VALUE) \
avi::call avi::SubElements_##_VALUE(uint64_t Name) \
{ \
    switch (Name) \
    { \

#define ELEMENT_CASE(_VALUE,_NAME) \
    case 0x##_VALUE:  Levels[Level].SubElements = &avi::SubElements_##_NAME;  return &avi::_NAME;

#define ELEMENT_VOID(_VALUE,_NAME) \
    case 0x##_VALUE:  Levels[Level].SubElements = &avi::SubElements_Void;  return &avi::_NAME;


#define ELEMENT_END() \
    default:                        return SubElements_Void(Name); \
    } \
} \

ELEMENT_BEGIN(_)
ELEMENT_CASE(5249464641564920LL, AVI_)
ELEMENT_CASE(5249464641564958LL, AVIX)
ELEMENT_END()

ELEMENT_BEGIN(AVI_)
ELEMENT_CASE(4C4953546864726CLL, AVI__hdrl)
ELEMENT_CASE(4C4953546D6F7669LL, AVI__movi)
ELEMENT_END()

ELEMENT_BEGIN(AVI__hdrl)
ELEMENT_CASE(4C4953547374726CLL, AVI__hdrl_strl)
ELEMENT_END()

ELEMENT_BEGIN(AVI__hdrl_strl)
ELEMENT_VOID(73747266, AVI__hdrl_strl_strf)
ELEMENT_VOID(73747268, AVI__hdrl_strl_strh)
ELEMENT_END()

ELEMENT_BEGIN(AVI__movi)
ELEMENT_VOID(30306463, AVI__movi_00dc)
ELEMENT_VOID(30317762, AVI__movi_01wb)
ELEMENT_END()

ELEMENT_BEGIN(AVIX)
ELEMENT_CASE(4C4953546D6F7669LL, AVI__movi)
ELEMENT_END()

ELEMENT_BEGIN(AVIX_movi)
ELEMENT_VOID(30306463, AVI__movi_00dc)
ELEMENT_VOID(30317762, AVI__movi_01wb)
ELEMENT_END()

//---------------------------------------------------------------------------
avi::call avi::SubElements_Void(uint64_t /*Name*/)
{
    Levels[Level].SubElements = &avi::SubElements_Void; return &avi::Void;
}

//***************************************************************************
// AVI
//***************************************************************************

//---------------------------------------------------------------------------
avi::avi(errors* Errors_Source) :
    input_base_uncompressed_compound(Errors_Source, Parser_AVI)
{
}

//---------------------------------------------------------------------------
void avi::ParseBuffer()
{
    if (Buffer.Size() < 12)
        return;
    if (Buffer[8] != 'A' || Buffer[9] != 'V' || Buffer[10] != 'I' || Buffer[11] != ' ')
        return;
    if (Buffer[0] != 'R' || Buffer[1] != 'I' || Buffer[2] != 'F' || Buffer[3] != 'F')
        return;
    SetDetected();

    Flavor = (decltype(Flavor))-1;
    Buffer_Offset = 0;
    Levels[0].Offset_End = Buffer.Size();
    Levels[0].SubElements = &avi::SubElements__;
    Level=1;

    while (Buffer_Offset < Buffer.Size())
    {
        // Find the current nesting level
        while (Buffer_Offset >= Levels[Level - 1].Offset_End)
        {
            Levels[Level].SubElements = NULL;
            Level--;
        }
        uint64_t End = Levels[Level - 1].Offset_End;

        // Parse the chunk header
        uint64_t Name;
        uint64_t Size;
        if (Buffer_Offset + 8 > End)
        {
            Undecodable(undecodable::TruncatedChunk);
            return;
        }
        Name = Get_B4();
        Size = Get_L4();
        switch (Name)
        {
        case 0x4C495354: // "LIST"
        case 0x52494646: // "RIFF"
            if (Size < 4 || Buffer_Offset + 4 > End)
            {
                Undecodable(undecodable::RIFF_ChunkSize);
                return;
            }
            Name <<= 32;
            Name |= Get_B4();
            Size -= 4;
        }
        if (Buffer_Offset + Size > End)
        {
            if (!Actions[Action_AcceptTruncated])
            {
                Undecodable(undecodable::TruncatedChunk);
                return;
            }
            //Size = Levels[Level - 1].Offset_End - Buffer_Offset;
        }

        // Parse the chunk content
        Levels[Level].Offset_End = Buffer_Offset + Size;
        call Call = (this->*Levels[Level - 1].SubElements)(Name);
        IsList = false;
        (this->*Call)();
        if (!IsList)
        {
            Buffer_Offset = Levels[Level].Offset_End;

            // Padding byte
            if (Buffer_Offset % 2 && Buffer_Offset < Buffer.Size() && !Buffer[Buffer_Offset])
            {
                Buffer_Offset++;
                Levels[Level].Offset_End = Buffer_Offset;
            }
        }

        // Next chunk (or sub-chunk)
        if (Buffer_Offset < Levels[Level].Offset_End)
            Level++;
    }

    // Can we compress?
    if (!HasErrors())
        SetSupported();

    // Write RAWcooked file
    if (IsSupported() && RAWcooked)
    {
        // Last part
        auto InSize = Buffer_Offset - Buffer_LastPos;
        memcpy(In + In_Pos, Buffer.Data() + Buffer_LastPos, InSize);
        In_Pos += InSize;
        Buffer_LastPos = Levels[Level].Offset_End;

        RAWcooked->Unique = true;
        RAWcooked->BeforeData = nullptr;
        RAWcooked->BeforeData_Size = 0;
        RAWcooked->AfterData = nullptr;
        RAWcooked->AfterData_Size =0;
        RAWcooked->InData = In;
        RAWcooked->InData_Size = In_Pos;
        RAWcooked->FileSize = FileSize;
        if (Actions[Action_Hash])
        {
            Hash();
            RAWcooked->HashValue = &HashValue;
        }
        else
            RAWcooked->HashValue = nullptr;
        RAWcooked->IsAttachment = false;
        RAWcooked->IsContainer = true;
        auto SeparatorPos = RAWcooked->OutputFileName.find('/');
        if (SeparatorPos != (size_t)-1)
            RAWcooked->OutputFileName.erase(0, SeparatorPos + 1); // TODO: more generic removal of directory name for unique files
        RAWcooked->Parse();
    }
}

//---------------------------------------------------------------------------
void avi::Void()
{
}

//---------------------------------------------------------------------------
void avi::AVI_()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void avi::AVI__hdrl()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void avi::AVI__hdrl_strl()
{
    Tracks.resize(Tracks.size() + 1);
    IsList = true;
}

//---------------------------------------------------------------------------
void avi::AVI__hdrl_strl_strf()
{
    auto& Track = Tracks.back();

    switch (Track.fccType)
    {
        case 0x61756473 : AVI__hdrl_strl_strf_auds(); break;
        case 0x76696473 : AVI__hdrl_strl_strf_vids(); break;
        default         : Unsupported(unsupported::fmt__extension);
    }
}

//---------------------------------------------------------------------------
// TODO: copy of WAVE_fmt_, should be common code
void avi::AVI__hdrl_strl_strf_auds()
{
    if (Levels[Level].Offset_End - Buffer_Offset < 16)
    {
        Undecodable(undecodable::fmt__ChunkSize);
        return;
    }

    uint16_t FormatTag = Get_L2();
    uint16_t Channels = Get_L2();
    uint32_t SamplesPerSec = Get_L4();
    uint32_t AvgBytesPerSec = Get_L4();
    BlockAlign = Get_L2();
    uint16_t BitDepth = Get_L2();

    if (AvgBytesPerSec * 8 != Channels * BitDepth * SamplesPerSec)
        Unsupported(unsupported::fmt__AvgBytesPerSec);
    if (BlockAlign * 8 != Channels * BitDepth)
        Unsupported(unsupported::fmt__BlockAlign);
    if (FormatTag == 1)
    {
        // Some files have zeroes after actual fmt content, it does not hurt so we accept them
        while (Buffer_Offset + 2 <= Levels[Level].Offset_End)
        {
            uint16_t Padding0 = Get_L2();
            if (Padding0)
            {
                Unsupported(unsupported::fmt__extension);
                return;
            }
        }

        if (Levels[Level].Offset_End - Buffer_Offset)
        {
            Unsupported(unsupported::fmt__extension);
            return;
        }
    }
    if (FormatTag == 0xFFFE)
    {
        if (Levels[Level].Offset_End - Buffer_Offset != 24)
        {
            Unsupported(unsupported::fmt__extension);
            return;
        }
        uint16_t cbSize = Get_L2();
        if (cbSize != 22)
        {
            Unsupported(unsupported::fmt__cbSize);
            return;
        }
        uint16_t ValidBitsPerSample = Get_L2();
        if (ValidBitsPerSample != BitDepth)
            Unsupported(unsupported::fmt__ValidBitsPerSample);
        uint32_t ChannelMask = Get_L4();
        if ((Channels != 1 || (ChannelMask != 0x00000000 && ChannelMask != 0x00000004))
         && (Channels != 2 || (ChannelMask != 0x00000000 && ChannelMask != 0x00000003))
         && (Channels != 6 || (ChannelMask != 0x00000000 && ChannelMask != 0x0000003F && ChannelMask != 0x0000060F))
         && (Channels != 8 || (ChannelMask != 0x00000000 && ChannelMask != 0x0000063F)))
            Unsupported(unsupported::fmt__ChannelMask);
        FormatTag = Get_L4();
        uint32_t SubFormat2 = Get_L4();
        uint32_t SubFormat3 = Get_B4();
        uint64_t SubFormat4 = Get_B4();
        if (SubFormat2 != 0x00100000
         || SubFormat3 != 0x800000aa
         || SubFormat4 != 0x00389b71)
            Unsupported(unsupported::fmt__SubFormat);
    }

    // Supported?
    auto WavFlavor = Wav_CheckSupported(FormatTag, Channels, SamplesPerSec, BitDepth);
    if (WavFlavor == (decltype(WavFlavor))-1)
        Unsupported(unsupported::Flavor);
    if (HasErrors())
        return;

    if (InputInfo && !InputInfo->SampleRate)
        InputInfo->SampleRate = SamplesPerSec;

    // AVI specific checks
    auto& Track = Tracks.back();
    if (Track.fccHandler && !(Track.fccHandler && 0xFFFF) && ((FormatTag >> 8) | (FormatTag << 8)) != Track.fccHandler)
        Unsupported(unsupported::Flavor);
}

//---------------------------------------------------------------------------
void avi::AVI__hdrl_strl_strf_vids()
{
    if (Levels[Level].Offset_End - Buffer_Offset != 40)
    {
        Unsupported(unsupported::Flavor);
        return;
    }
    uint32_t Size = Get_L4();
    if (Size != 40)
    {
        Unsupported(unsupported::Flavor);
        return;
    }
    Width = Get_L4();
    Height = Get_L4();
    uint16_t Planes = Get_L2();
    uint16_t BitCount = Get_L2();
    uint32_t Compression = Get_B4();
    uint32_t SizeOfImage = Get_L4();

    auto& Track = Tracks.back();
    if (Track.fccHandler && Compression != Track.fccHandler)
        Unsupported(unsupported::Flavor);

    // Supported?
    if (Compression != 0x76323130)
        Unsupported(unsupported::Flavor);
    else if (Width % 48)
        Unsupported(unsupported::Flavor);
    else if (BitCount != 24 && BitCount != 20) // v210 spec specifies 24 but in reality it is 20, we accept both
        Unsupported(unsupported::Flavor);
    else if (Width * Height * 20 != SizeOfImage * 8 && Width * Height * 20 * 32 / 30 != SizeOfImage * 8) // Either with or without padding is found, accpeting both
        Unsupported(unsupported::Flavor);
    if (HasErrors())
        return;
    Flavor = (decltype(Flavor))flavor::v210;

    // Slices count
    // Computing optimal count of slices. TODO: agree with everyone about the goal and/or permit multiple formulas
    // Current idea:
    // - have some SIMD compatible slice count (e.g. AVX-512 has 16 32-bit blocks, let's take multiples of 16)
    // - each slice has around 256 KiB of data, there is a similar risk of losing 1 LTO block (correction code per block, to be confirmed but looks like a 256 KiB block size is classic and LTFS 2.4 spec indicates 512 KiB in the example)
    // This leads to:
    // SD: 16 slices (10-bit) or 24 slices (16-bit)
    // HD/2K: 64 slices (10-bit) or 96 slices (16-bit)
    // UHD/4K: 256 slices (10-bit) or 384 slices (16-bit)
    // 
    slice_x = 4;
    if (Width >= 1440) // more than 2/3 of 1920, so e.g. DV100 is included
        slice_x <<= 1;
    if (Width >= 2880) // more than 3/2 of 1920, oversampled HD is not included
        slice_x <<= 1;
    if (slice_x > Width / 2)
        slice_x = Width / 2;
    if (slice_x > Height / 2)
        slice_x = Height / 2;
    if (!slice_x)
        slice_x = 1;

    // Computing which slice count is suitable
    size_t Slice_Multiplier = PixelsPerBlock((flavor)Flavor);
    if (Slice_Multiplier > 1)
    {
        // Temporary limitation because the decoder does not support yet the merge of data from 2 slices in one DPX block
        for (; slice_x; slice_x--)
        {
            if (Width % (slice_x * Slice_Multiplier) == 0)
                break;
        }
        if (slice_x == 0)
            Unsupported(unsupported::PixelBoundaries);
    }
    slice_y = slice_x;
}

//---------------------------------------------------------------------------
void avi::AVI__hdrl_strl_strh()
{
    if (Levels[Level].Offset_End - Buffer_Offset != 56)
    {
        Unsupported(unsupported::fmt__extension);
        return;
    }
    auto& Track = Tracks.back();

    Track.fccType = Get_B4();
    Track.fccHandler = Get_B4();
}

//---------------------------------------------------------------------------
void avi::AVI__movi()
{
    // Test if fmt chunk was parsed
    if (!HasErrors() && (Tracks.size() != 2 || Tracks[0].fccType != 0x76696473 || Tracks[1].fccType != 0x61756473))
        Unsupported(unsupported::Flavor);

    if (HasErrors())
        Buffer_Offset = FileSize;

    IsList = true;
}

//---------------------------------------------------------------------------
void avi::AVI__movi_00dc()
{
    if (Actions[Action_AcceptTruncated])
    {
        if (Input.Empty())
            Input.Create(Buffer);
        auto Diff = Levels[Level].Offset_End - Buffer_Offset;
        position Position;
        Position.Index = 0;
        Position.Size = (uint32_t)Diff;
        Position.Input_Offset = Buffer_Offset;
        Position.Output_Offset = Buffer_Offset + InputOutput_Diff;
        Position.Buffer = nullptr;
        Positions.push_back(Position);
        InputOutput_Diff += Diff;
        for (int i = (int)Level; i > 0 ; i--)
            Levels[i].Offset_End -= Diff;
        return;
    }

    //if (Width * Height * 8 / 3)

    if (!In)
        In = new uint8_t[1000000000];

    auto InSize = Buffer_Offset - Buffer_LastPos;
    memcpy(In + In_Pos, Buffer.Data() + Buffer_LastPos, InSize);
    In_Pos += InSize;
    Buffer_LastPos = Levels[Level].Offset_End;
}

//---------------------------------------------------------------------------
void avi::AVI__movi_01wb()
{
    if (Actions[Action_AcceptTruncated])
    {
        if (Input.Empty())
            Input.Create(Buffer);
        auto Diff = Levels[Level].Offset_End - Buffer_Offset;
        position Position;
        Position.Index = 1;
        Position.Size = (uint32_t)Diff;
        Position.Input_Offset = Buffer_Offset;
        Position.Output_Offset = Buffer_Offset + InputOutput_Diff;
        Position.Buffer = nullptr;
        Positions.push_back(Position);
        InputOutput_Diff += Diff;
        for (int i = (int)Level; i > 0; i--)
            Levels[i].Offset_End -= Diff;
        return;
    }

    if (!In)
        In = new uint8_t[1000000000];

    auto InSize = Buffer_Offset - Buffer_LastPos;
    memcpy(In + In_Pos, Buffer.Data() + Buffer_LastPos, InSize);
    In_Pos += InSize;
    Buffer_LastPos = Levels[Level].Offset_End;
}

//---------------------------------------------------------------------------
void avi::AVIX()
{
    AVI_();
}

//---------------------------------------------------------------------------
void avi::AVIX_movi()
{
    AVI__movi();
}

//---------------------------------------------------------------------------
void avi::AVIX_movi_00dc()
{
    AVI__movi_00dc();
}

//---------------------------------------------------------------------------
void avi::AVIX_movi_01wb()
{
    AVI__movi_01wb();
}

//---------------------------------------------------------------------------
void avi::BufferOverflow()
{
    Undecodable(undecodable::BufferOverflow);
}

//---------------------------------------------------------------------------
string avi::Flavor_String()
{
    return AVI_Flavor_String((uint8_t)Flavor);
}

//---------------------------------------------------------------------------
uint8_t avi::BitDepth()
{
    return WAV_BitDepth((wav::flavor)Flavor);
}

//---------------------------------------------------------------------------
sign avi::Sign()
{
    return WAV_Sign((wav::flavor)Flavor);
}

//---------------------------------------------------------------------------
endianness avi::Endianness()
{
    return WAV_Endianness((wav::flavor)Flavor);
}

//---------------------------------------------------------------------------
string AVI_Flavor_String(uint8_t Flavor)
{
    string ToReturn("AVI/v210");
    return ToReturn;
}

//---------------------------------------------------------------------------
size_t avi::GetStreamCount()
{
    return Tracks.size();
}

//---------------------------------------------------------------------------
size_t avi::BytesPerBlock(avi::flavor Flavor)
{
    return 32;
}

//---------------------------------------------------------------------------
size_t avi::PixelsPerBlock(avi::flavor Flavor)
{
    return 12;
}
