/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/RIFF/RIFF.h"
#include "Lib/RAWcooked/RAWcooked.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Tested cases
struct riff_tested
{
    uint32_t                    SamplesPerSec;
    uint8_t                     BitDepth;
    uint8_t                     Channels;
    riff::endianess             Endianess;
    riff::style                 Style;
};

const size_t RIFF_Tested_Size = 27;
struct riff_tested RIFF_Tested[RIFF_Tested_Size] =
{
    { 44100,  8, 1, riff::LE, riff::PCM_44100_8_1_U },
    { 44100,  8, 2, riff::LE, riff::PCM_44100_8_2_U },
    { 44100,  8, 6, riff::LE, riff::PCM_44100_8_6_U },
    { 44100, 16, 1, riff::LE, riff::PCM_44100_16_1_LE },
    { 44100, 16, 2, riff::LE, riff::PCM_44100_16_2_LE },
    { 44100, 16, 6, riff::LE, riff::PCM_44100_16_6_LE },
    { 44100, 24, 1, riff::LE, riff::PCM_44100_24_1_LE },
    { 44100, 24, 2, riff::LE, riff::PCM_44100_24_2_LE },
    { 44100, 24, 6, riff::LE, riff::PCM_44100_24_6_LE },
    { 48000,  8, 1, riff::LE, riff::PCM_44100_8_1_U },
    { 48000,  8, 2, riff::LE, riff::PCM_44100_8_2_U },
    { 48000,  8, 6, riff::LE, riff::PCM_44100_8_6_U },
    { 48000, 16, 1, riff::LE, riff::PCM_48000_16_1_LE },
    { 48000, 16, 2, riff::LE, riff::PCM_48000_16_2_LE },
    { 48000, 16, 6, riff::LE, riff::PCM_48000_16_6_LE },
    { 48000, 24, 1, riff::LE, riff::PCM_48000_24_1_LE },
    { 48000, 24, 2, riff::LE, riff::PCM_48000_24_2_LE },
    { 48000, 24, 6, riff::LE, riff::PCM_48000_24_6_LE },
    { 96000,  8, 1, riff::LE, riff::PCM_44100_8_1_U },
    { 96000,  8, 2, riff::LE, riff::PCM_44100_8_2_U },
    { 96000,  8, 6, riff::LE, riff::PCM_44100_8_6_U },
    { 96000, 16, 1, riff::LE, riff::PCM_96000_16_1_LE },
    { 96000, 16, 2, riff::LE, riff::PCM_96000_16_2_LE },
    { 96000, 16, 6, riff::LE, riff::PCM_96000_16_6_LE },
    { 96000, 24, 1, riff::LE, riff::PCM_96000_24_1_LE },
    { 96000, 24, 2, riff::LE, riff::PCM_96000_24_2_LE },
    { 96000, 24, 6, riff::LE, riff::PCM_96000_24_6_LE },
};

//---------------------------------------------------------------------------


#define ELEMENT_BEGIN(_VALUE) \
riff::call riff::SubElements_##_VALUE(uint64_t Name) \
{ \
    switch (Name) \
    { \

#define ELEMENT_CASE(_VALUE,_NAME) \
    case 0x##_VALUE:  Levels[Level].SubElements = &riff::SubElements_##_NAME;  return &riff::_NAME;

#define ELEMENT_VOID(_VALUE,_NAME) \
    case 0x##_VALUE:  Levels[Level].SubElements = &riff::SubElements_Void;  return &riff::_NAME;


#define ELEMENT_END() \
    default:                        return SubElements_Void(Name); \
    } \
} \

ELEMENT_BEGIN(_)
ELEMENT_CASE(57415645, WAVE)
ELEMENT_END()

ELEMENT_BEGIN(WAVE)
ELEMENT_VOID(64617461, WAVE_data)
ELEMENT_VOID(666D7420, WAVE_fmt_)
ELEMENT_END()


//---------------------------------------------------------------------------
riff::call riff::SubElements_Void(uint64_t Name)
{
    Levels[Level].SubElements = &riff::SubElements_Void; return &riff::Void;
}

//***************************************************************************
// Errors
//***************************************************************************

//---------------------------------------------------------------------------
const char* riff::ErrorMessage()
{
    return error_message;
}

//***************************************************************************
// RIFF
//***************************************************************************

//---------------------------------------------------------------------------
riff::riff() :
    RAWcooked(NULL),
    IsDetected(false),
    Style(Style_Max),
    error_message(NULL)
{
}

//---------------------------------------------------------------------------
uint16_t riff::Get_L2()
{
    uint16_t ToReturn = Buffer[Buffer_Offset + 0] | (Buffer[Buffer_Offset + 1] << 8);
    Buffer_Offset += 2;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint32_t riff::Get_L4()
{
    uint32_t ToReturn = Buffer[Buffer_Offset+0] | (Buffer[Buffer_Offset + 1] << 8) | (Buffer[Buffer_Offset + 2] << 16) | (Buffer[Buffer_Offset + 3] << 24);
    Buffer_Offset += 4;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint32_t riff::Get_B4()
{
    uint32_t ToReturn = (Buffer[Buffer_Offset + 0] << 24) | (Buffer[Buffer_Offset + 1] << 16) | (Buffer[Buffer_Offset + 2] << 8) | Buffer[Buffer_Offset + 3];
    Buffer_Offset += 4;
    return ToReturn;
}

//---------------------------------------------------------------------------
bool riff::Parse(bool AcceptTruncated)
{
    if (Buffer_Size < 12)
        return false;
    if (Buffer[8] != 'W' || Buffer[9] != 'A' || Buffer[10] != 'V' || Buffer[11] != 'E')
        return false;
    if (Buffer[0] == 'R' && Buffer[1] == 'F' && Buffer[2] == '6' && Buffer[3] == '4')
    {
        IsDetected = true;
        Error("RF64 (4GB+ WAV)");
        return false;
    }
    if (Buffer[0] != 'R' || Buffer[1] != 'I' || Buffer[2] != 'F' || Buffer[3] != 'F')
        return false;
    IsDetected = true;

    Buffer_Offset = 0;
    Levels[0].Offset_End = Buffer_Size;
    Levels[0].SubElements = &riff::SubElements__;
    Level=1;

    while (Buffer_Offset < Buffer_Size)
    {
        // Find the current nesting level
        while (Buffer_Offset >= Levels[Level - 1].Offset_End)
        {
            Levels[Level].SubElements = NULL;
            Level--;
        }

        // Parse the chunk header
        uint32_t Name;
        uint64_t Size;
        if (Buffer_Offset + 8 <= Levels[Level - 1].Offset_End)
        {
            Name = Get_B4();
            Size = Get_L4();
            if (Name == 0x52494646) // "RIFF"
            {
                if (Size < 4)
                {
                    Error("Incoherency detected while parsing RIFF");
                    return false;
                }
                Name = Get_B4();
                Size -= 4;
            }
            if (Buffer_Offset + Size > Levels[Level - 1].Offset_End)
            {
                // Truncated
                if (!AcceptTruncated)
                {
                    Error("Truncated RIFF?");
                    return false;
                }
                Size = Levels[Level - 1].Offset_End - Buffer_Offset;
            }
        }
        else
        {
            // There is a problem in the chunk
            Error("Incoherency detected while parsing RIFF");
            return false;
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
            if (Buffer_Offset % 2 && Buffer_Offset < Buffer_Size && !Buffer[Buffer_Offset])
            {
                Buffer_Offset++;
                Levels[Level].Offset_End = Buffer_Offset;
            }
        }

        // Next chunk (or sub-chunk)
        if (Buffer_Offset < Levels[Level].Offset_End)
            Level++;
    }

    return 0;
}

//---------------------------------------------------------------------------
uint8_t riff::BitDepth()
{
    return RIFF_Tested[Style].BitDepth;
}

//---------------------------------------------------------------------------
riff::endianess riff::Endianess()
{
    return RIFF_Tested[Style].Endianess;
}

//---------------------------------------------------------------------------
void riff::Void()
{
}

//---------------------------------------------------------------------------
void riff::WAVE()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void riff::WAVE_data()
{
    // Write RAWcooked file
    if (RAWcooked)
    {
        RAWcooked->Unique = true;
        RAWcooked->Before = Buffer;
        RAWcooked->Before_Size = Buffer_Offset;
        RAWcooked->After = Buffer + Levels[Level].Offset_End;
        RAWcooked->After_Size = Buffer_Size - Levels[Level].Offset_End;
        RAWcooked->Parse();
    }
}

//---------------------------------------------------------------------------
void riff::WAVE_fmt_()
{
    if (Levels[Level].Offset_End - Buffer_Offset < 16)
    {
        Error("WAV FormatTag format");
        return;
    }

    endianess Endianess;
    uint16_t FormatTag = Get_L2();
    if (FormatTag == 1 || FormatTag == 0xFFFE)
        Endianess = LE;
    else
    {
        Error("WAV FormatTag is not WAVE_FORMAT_PCM 1");
        return;
    }

    uint16_t Channels = Get_L2();
    uint32_t SamplesPerSec = Get_L4();
    uint32_t AvgBytesPerSec = Get_L4();
    uint16_t BlockAlign = Get_L2();
    uint16_t BitDepth = Get_L2();

    if (AvgBytesPerSec * 8 != Channels * BitDepth * SamplesPerSec)
    {
        Error("WAV BlockAlign not supported");
        return;
    }
    if (BlockAlign * 8 != Channels * BitDepth)
    {
        Error("WAV BlockAlign not supported");
        return;
    }
    if (FormatTag == 1)
    {
        if (Levels[Level].Offset_End == Buffer_Offset + 2)
        {
            uint16_t Padding0 = Get_L2(); // Some files have 2 zeroes, it does not hurt so we accept them
            if (Padding0)
            {
                Error("WAV FormatTag extension");
                return;
            }
        }

        if (Levels[Level].Offset_End == Buffer_Offset + 4)
        {
            uint32_t Padding0 = Get_L4(); // Some files have 4 zeroes, it does not hurt so we accept them
            if (Padding0)
            {
                Error("WAV FormatTag extension");
                return;
            }
        }

        if (Levels[Level].Offset_End - Buffer_Offset)
        {
            Error("WAV FormatTag extension");
            return;
        }
    }
    if (FormatTag == 0xFFFE)
    {
        if (Levels[Level].Offset_End - Buffer_Offset != 24)
        {
            Error("WAV FormatTag extension");
            return;
        }
        uint16_t cbSize = Get_L2();
        if (cbSize != 22)
        {
            Error("WAV FormatTag cbSize");
            return;
        }
        uint16_t ValidBitsPerSample = Get_L2();
        if (ValidBitsPerSample != BitDepth)
        {
            Error("WAV FormatTag ValidBitsPerSample");
            return;
        }
        uint32_t ChannelMask = Get_L4();
        if ((Channels != 1 || (ChannelMask != 0x00000000 && ChannelMask != 0x00000004))
         && (Channels != 2 || (ChannelMask != 0x00000000 && ChannelMask != 0x00000003))
         && (Channels != 6 || (ChannelMask != 0x00000000 && ChannelMask != 0x0000003F && ChannelMask != 0x0000060F)))
        {
            Error("WAV FormatTag ChannelMask");
            return;
        }
        uint32_t SubFormat1 = Get_L4();
        uint32_t SubFormat2 = Get_L4();
        uint32_t SubFormat3 = Get_B4();
        uint64_t SubFormat4 = Get_B4();
        if (SubFormat1 != 0x00000001
         || SubFormat2 != 0x00100000
         || SubFormat3 != 0x800000aa
         || SubFormat4 != 0x00389b71)
        {
            Error("WAV SubFormat is not KSDATAFORMAT_SUBTYPE_PCM 00000001-0000-0010-8000-00AA00389B71");
            return;
        }
    }

    // Supported?
    size_t Tested = 0;
    for (; Tested < RIFF_Tested_Size; Tested++)
    {
        riff_tested& RIFF_Tested_Item = RIFF_Tested[Tested];
        if (RIFF_Tested_Item.SamplesPerSec == SamplesPerSec
            && RIFF_Tested_Item.BitDepth == BitDepth
            && RIFF_Tested_Item.Channels == Channels
            && RIFF_Tested_Item.Endianess == Endianess)
            break;
    }
    if (Tested >= RIFF_Tested_Size)
    {
        Error("Style (SamplesPerSec / BitDepth / Channels / Endianess combination)");
        return;
    }
    Style = RIFF_Tested[Tested].Style;
}

//---------------------------------------------------------------------------
uint32_t riff::SamplesPerSec(riff::style Style)
{
    switch (Style)
    {
        case PCM_44100_8_1_U:
        case PCM_44100_8_2_U:
        case PCM_44100_8_6_U:
        case PCM_44100_16_1_LE:
        case PCM_44100_16_2_LE:
        case PCM_44100_16_6_LE:
        case PCM_44100_24_1_LE:
        case PCM_44100_24_2_LE:
        case PCM_44100_24_6_LE:
                                        return 44100;
        case PCM_48000_8_1_U:
        case PCM_48000_8_2_U:
        case PCM_48000_8_6_U:
        case PCM_48000_16_1_LE:
        case PCM_48000_16_2_LE:
        case PCM_48000_16_6_LE:
        case PCM_48000_24_1_LE:
        case PCM_48000_24_2_LE:
        case PCM_48000_24_6_LE:
                                        return 48000;
        case PCM_96000_8_1_U:
        case PCM_96000_8_2_U:
        case PCM_96000_8_6_U:
        case PCM_96000_16_1_LE:
        case PCM_96000_16_2_LE:
        case PCM_96000_16_6_LE:
        case PCM_96000_24_1_LE:
        case PCM_96000_24_2_LE:
        case PCM_96000_24_6_LE:
                                        return 96000;
        default:
                                        return 0;
    }
}
const char* riff::SamplesPerSec_String(riff::style Style)
{
    uint32_t Value = riff::SamplesPerSec(Style);

    switch (Value)
    {
        case 44100:
                                        return "44";
        case 48000:
                                        return "48";
        case 96000:
                                        return "96";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
uint8_t riff::BitDepth(riff::style Style)
{
    switch (Style)
    {
        case PCM_44100_8_1_U:
        case PCM_44100_8_2_U:
        case PCM_44100_8_6_U:
        case PCM_48000_8_1_U:
        case PCM_48000_8_2_U:
        case PCM_48000_8_6_U:
        case PCM_96000_8_1_U:
        case PCM_96000_8_2_U:
        case PCM_96000_8_6_U:
                                        return 8;
        case PCM_44100_16_1_LE:
        case PCM_44100_16_2_LE:
        case PCM_44100_16_6_LE:
        case PCM_48000_16_1_LE:
        case PCM_48000_16_2_LE:
        case PCM_48000_16_6_LE:
        case PCM_96000_16_1_LE:
        case PCM_96000_16_2_LE:
        case PCM_96000_16_6_LE:
                                        return 16;
        case PCM_44100_24_1_LE:
        case PCM_44100_24_2_LE:
        case PCM_44100_24_6_LE:
        case PCM_48000_24_1_LE:
        case PCM_48000_24_2_LE:
        case PCM_48000_24_6_LE:
        case PCM_96000_24_1_LE:
        case PCM_96000_24_2_LE:
        case PCM_96000_24_6_LE:
                                        return 24;
        default:
                                        return 0;
    }
}
const char* riff::BitDepth_String(riff::style Style)
{
    uint8_t Value = riff::BitDepth(Style);

    switch (Value)
    {
        case 8:
                                        return "8";
        case 16:
                                        return "16";
        case 24:
                                        return "24";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
uint8_t riff::Channels(riff::style Style)
{
    switch (Style)
    {
        case PCM_44100_8_1_U:
        case PCM_48000_8_1_U:
        case PCM_96000_8_1_U:
        case PCM_44100_16_1_LE:
        case PCM_48000_16_1_LE:
        case PCM_96000_16_1_LE:
        case PCM_48000_24_1_LE:
        case PCM_44100_24_1_LE:
        case PCM_96000_24_1_LE:
                                        return 1;
        case PCM_44100_8_2_U:
        case PCM_48000_8_2_U:
        case PCM_96000_8_2_U:
        case PCM_44100_16_2_LE:
        case PCM_48000_16_2_LE:
        case PCM_96000_16_2_LE:
        case PCM_44100_24_2_LE:
        case PCM_48000_24_2_LE:
        case PCM_96000_24_2_LE:
                                        return 2;
        case PCM_44100_8_6_U:
        case PCM_48000_8_6_U:
        case PCM_96000_8_6_U:
        case PCM_44100_16_6_LE:
        case PCM_48000_16_6_LE:
        case PCM_96000_16_6_LE:
        case PCM_44100_24_6_LE:
        case PCM_48000_24_6_LE:
        case PCM_96000_24_6_LE:
                                        return 6;
        default:
                                        return 0;
    }
}
const char* riff::Channels_String(riff::style Style)
{
    uint8_t Value = riff::Channels(Style);

    switch (Value)
    {
        case 1:
                                        return "1";
        case 2:
                                        return "2";
        case 6:
                                        return "6";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
riff::endianess riff::Endianess(riff::style Style)
{
    return LE; //For the moment all is LE or Unsigned
}
const char* riff::Endianess_String(riff::style Style)
{
    riff::endianess Value = riff::Endianess(Style);
    uint8_t BitDepth = riff::BitDepth(Style);

    switch (Value)
    {
        case LE:
                                        return BitDepth==8?"U":"LE";
        case BE:
                                        return BitDepth==8?"S":"BE";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
string riff::Flavor_String(riff::style Style)
{
    string ToReturn("WAV/PCM/");
    ToReturn += SamplesPerSec_String(Style);
    ToReturn += "kHz/";
    ToReturn += BitDepth_String(Style);
    ToReturn += "bit/";
    ToReturn += Channels_String(Style);
    ToReturn += "ch";
    const char* Value = Endianess_String(Style);
    if (Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    return ToReturn;
}

