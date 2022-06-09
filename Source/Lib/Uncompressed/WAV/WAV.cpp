/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Uncompressed/WAV/WAV.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace wav_issue {

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
    "RF64 (4GB+ WAV)",
    "fmt FormatTag not WAVE_FORMAT_PCM 1",
    "fmt AvgBytesPerSec",
    "fmt BlockAlign",
    "fmt extension",
    "fmt cbSize",
    "fmt ValidBitsPerSample",
    "fmt ChannelMask",
    "fmt SubFormat not KSDATAFORMAT_SUBTYPE_PCM 00000001-0000-0010-8000-00AA00389B71",
    "fmt chunk not before data chunk",
    "Flavor (SamplesPerSec / BitDepth / Channels / Endianness combination)",
    "data chunk size is not a multiple of BlockAlign",
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
    data_Size,
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

} // wav_issue

using namespace wav_issue;

//---------------------------------------------------------------------------
// Tested cases
struct wav_tested
{
    samplerate_code             SamplesPerSecCode;
    bitdepth                    BitDepth;
    channels                    Channels;
    sign                        Sign;

    bool operator == (const wav_tested& Value) const
    {
        return SamplesPerSecCode == Value.SamplesPerSecCode
            && BitDepth == Value.BitDepth
            && Channels == Value.Channels
            && Sign == Value.Sign
            ;
    }
};

struct wav_tested WAV_Tested[] =
{
    { samplerate_code::_44100,  8, 1, sign::U },
    { samplerate_code::_44100,  8, 2, sign::U },
    { samplerate_code::_44100,  8, 6, sign::U },
    { samplerate_code::_44100,  8, 8, sign::U },
    { samplerate_code::_44100, 16, 1, sign::S },
    { samplerate_code::_44100, 16, 2, sign::S },
    { samplerate_code::_44100, 16, 6, sign::S },
    { samplerate_code::_44100, 16, 8, sign::S },
    { samplerate_code::_44100, 24, 1, sign::S },
    { samplerate_code::_44100, 24, 2, sign::S },
    { samplerate_code::_44100, 24, 6, sign::S },
    { samplerate_code::_44100, 24, 8, sign::S },
    { samplerate_code::_44100, 32, 1, sign::S },
    { samplerate_code::_44100, 32, 2, sign::S },
    { samplerate_code::_44100, 32, 6, sign::S },
    { samplerate_code::_44100, 32, 8, sign::S },
    { samplerate_code::_48000,  8, 1, sign::U },
    { samplerate_code::_48000,  8, 2, sign::U },
    { samplerate_code::_48000,  8, 6, sign::U },
    { samplerate_code::_48000,  8, 8, sign::U },
    { samplerate_code::_48000, 16, 1, sign::S },
    { samplerate_code::_48000, 16, 2, sign::S },
    { samplerate_code::_48000, 16, 6, sign::S },
    { samplerate_code::_48000, 16, 8, sign::S },
    { samplerate_code::_48000, 24, 1, sign::S },
    { samplerate_code::_48000, 24, 2, sign::S },
    { samplerate_code::_48000, 24, 6, sign::S },
    { samplerate_code::_48000, 24, 8, sign::S },
    { samplerate_code::_48000, 32, 1, sign::S },
    { samplerate_code::_48000, 32, 2, sign::S },
    { samplerate_code::_48000, 32, 6, sign::S },
    { samplerate_code::_48000, 32, 8, sign::S },
    { samplerate_code::_96000,  8, 1, sign::U },
    { samplerate_code::_96000,  8, 2, sign::U },
    { samplerate_code::_96000,  8, 6, sign::U },
    { samplerate_code::_96000,  8, 8, sign::U },
    { samplerate_code::_96000, 16, 1, sign::S },
    { samplerate_code::_96000, 16, 2, sign::S },
    { samplerate_code::_96000, 16, 6, sign::S },
    { samplerate_code::_96000, 16, 8, sign::S },
    { samplerate_code::_96000, 24, 1, sign::S },
    { samplerate_code::_96000, 24, 2, sign::S },
    { samplerate_code::_96000, 24, 6, sign::S },
    { samplerate_code::_96000, 24, 8, sign::S },
    { samplerate_code::_96000, 32, 1, sign::S },
    { samplerate_code::_96000, 32, 2, sign::S },
    { samplerate_code::_96000, 32, 6, sign::S },
    { samplerate_code::_96000, 32, 8, sign::S },
};
static_assert(wav::flavor_Max == sizeof(WAV_Tested) / sizeof(wav_tested), IncoherencyMessage);

//---------------------------------------------------------------------------


#define ELEMENT_BEGIN(_VALUE) \
wav::call wav::SubElements_##_VALUE(uint64_t Name) \
{ \
    switch (Name) \
    { \

#define ELEMENT_CASE(_VALUE,_NAME) \
    case 0x##_VALUE:  Levels[Level].SubElements = &wav::SubElements_##_NAME;  return &wav::_NAME;

#define ELEMENT_VOID(_VALUE,_NAME) \
    case 0x##_VALUE:  Levels[Level].SubElements = &wav::SubElements_Void;  return &wav::_NAME;


#define ELEMENT_END() \
    default:                        return SubElements_Void(Name); \
    } \
} \

ELEMENT_BEGIN(_)
ELEMENT_CASE(5249464657415645LL, WAVE)
ELEMENT_END()

ELEMENT_BEGIN(WAVE)
ELEMENT_VOID(64617461, WAVE_data)
ELEMENT_VOID(666D7420, WAVE_fmt_)
ELEMENT_END()


//---------------------------------------------------------------------------
wav::call wav::SubElements_Void(uint64_t /*Name*/)
{
    Levels[Level].SubElements = &wav::SubElements_Void; return &wav::Void;
}

//***************************************************************************
// WAV
//***************************************************************************

//---------------------------------------------------------------------------
wav::wav(errors* Errors_Source) :
    input_base_uncompressed_audio(Errors_Source, Parser_WAV)
{
}

//---------------------------------------------------------------------------
void wav::ParseBuffer()
{
    if (Buffer.Size() < 12)
        return;
    if (Buffer[8] != 'W' || Buffer[9] != 'A' || Buffer[10] != 'V' || Buffer[11] != 'E')
        return;
    if (Buffer[0] == 'R' && Buffer[1] == 'F' && Buffer[2] == '6' && Buffer[3] == '4')
    {
        Unsupported(unsupported::RF64);
        return;
    }
    if (Buffer[0] != 'R' || Buffer[1] != 'I' || Buffer[2] != 'F' || Buffer[3] != 'F')
        return;
    SetDetected();

    Flavor = (decltype(Flavor))-1;
    Buffer_Offset = 0;
    Levels[0].Offset_End = Buffer.Size();
    Levels[0].SubElements = &wav::SubElements__;
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
        if (Name == 0x52494646) // "RIFF"
        {
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
            Size = Levels[Level - 1].Offset_End - Buffer_Offset;
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
}

//---------------------------------------------------------------------------
void wav::Void()
{
}

//---------------------------------------------------------------------------
void wav::WAVE()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void wav::WAVE_data()
{
    // Test if fmt chunk was parsed
    if (!HasErrors() && Flavor == (decltype(Flavor))-1)
        Unsupported(unsupported::fmt__Location);

    // Coherency test
    if (BlockAlign)
    {
        uint64_t Size = Levels[Level].Offset_End - Buffer_Offset;
        if (Size % BlockAlign)
            Unsupported(unsupported::data_Size);

        if (InputInfo && !InputInfo->SampleCount)
            InputInfo->SampleCount = Size / BlockAlign;
    }

    // Can we compress?
    if (!HasErrors())
        SetSupported();

    // Write RAWcooked file
    if (IsSupported() && RAWcooked)
    {
        RAWcooked->Unique = true;
        RAWcooked->BeforeData = Buffer.Data();
        RAWcooked->BeforeData_Size = Buffer_Offset;
        RAWcooked->AfterData = Buffer.Data() + Levels[Level].Offset_End;
        RAWcooked->AfterData_Size = Buffer.Size() - Levels[Level].Offset_End;
        RAWcooked->InData = nullptr;
        RAWcooked->InData_Size = 0;
        RAWcooked->FileSize = (uint64_t)-1;
        if (Actions[Action_Hash])
        {
            Hash();
            RAWcooked->HashValue = &HashValue;
        }
        else
            RAWcooked->HashValue = nullptr;
        RAWcooked->IsAttachment = false;
        RAWcooked->Parse();
    }
}

//---------------------------------------------------------------------------
void wav::WAVE_fmt_()
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
    if (FormatTag != 1 && FormatTag != 0xFFFE)
        Unsupported(unsupported::fmt__FormatTag);
    if ((FormatTag != 1 && FormatTag != 3)
     || Channels > (decltype(wav_tested::Channels))-1
     || BitDepth > (decltype(wav_tested::BitDepth))-1)
    {
        Unsupported(unsupported::Flavor);
        return;
    }
    wav_tested Info;
    Info.SamplesPerSecCode = SampleRate2Code(SamplesPerSec);
    Info.BitDepth = (decltype(wav_tested::BitDepth))BitDepth;
    Info.Channels = (decltype(wav_tested::Channels))Channels;
    Info.Sign = Info.BitDepth <= 8 ? sign::U: sign::S;
    for (const auto& WAV_Tested_Item : WAV_Tested)
    {
        if (WAV_Tested_Item == Info)
        {
            Flavor = (decltype(Flavor))(&WAV_Tested_Item - WAV_Tested);
            break;
        }
    }
    if (Flavor == (decltype(Flavor))-1)
        Unsupported(unsupported::Flavor);
    if (HasErrors())
        return;

    if (InputInfo && !InputInfo->SampleRate)
        InputInfo->SampleRate = SamplesPerSec;
}

//---------------------------------------------------------------------------
void wav::BufferOverflow()
{
    Undecodable(undecodable::BufferOverflow);
}

//---------------------------------------------------------------------------
string wav::Flavor_String()
{
    return WAV_Flavor_String(Flavor);
}

//---------------------------------------------------------------------------
uint8_t wav::BitDepth()
{
    return WAV_Tested[Flavor].BitDepth;
}

//---------------------------------------------------------------------------
sign wav::Sign()
{
    return WAV_Tested[Flavor].Sign;
}

//---------------------------------------------------------------------------
endianness wav::Endianness()
{
    return endianness::LE;
}

//---------------------------------------------------------------------------
string WAV_Flavor_String(uint8_t Flavor)
{
    const auto& Info = WAV_Tested[(size_t)Flavor];
    string ToReturn("WAV/");
    ToReturn += PCM_Flavor_String(Info.BitDepth, Info.Sign, endianness::LE, Info.Channels, Info.SamplesPerSecCode);
    return ToReturn;
}

