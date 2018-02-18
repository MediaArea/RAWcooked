/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/RIFF/RIFF.h"
#include "Lib/RAWcooked/RAWcooked.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Tested cases
enum endianess
{
    LE,
    BE,
};

struct riff_tested
{
    uint32_t                    SamplingRate;
    uint8_t                     BitDepth;
    uint32_t                    ChannelCount;
    endianess                   Endianess;
    riff::style                 Style;
};

const size_t RIFF_Tested_Size = 26;
struct riff_tested RIFF_Tested[RIFF_Tested_Size] =
{
    { 48000, 16, 2, LE, riff::PCM_48000_16_2_LE },
    { 48000, 16, 6, LE, riff::PCM_48000_16_6_LE },
    { 48000, 24, 2, LE, riff::PCM_48000_24_2_LE },
    { 48000, 24, 6, LE, riff::PCM_48000_24_6_LE },
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
// DPX
//***************************************************************************

//---------------------------------------------------------------------------
riff::riff() :
    WriteFileCall(NULL),
    WriteFileCall_Opaque(NULL),
    IsDetected(false),
    Style(PCM_Style_Max),
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
bool riff::Parse()
{
    if (Buffer_Size < 4 || Buffer[0] != 'R' || Buffer[1] != 'I' || Buffer[2] != 'F' || Buffer[3] != 'F')
        return false;

    IsDetected = true;
    Buffer_Offset = 0;
    Level = 0;

    Levels[Level].Offset_End = Buffer_Size;
    Levels[Level].SubElements = &riff::SubElements__;
    Level++;

    while (Buffer_Offset < Buffer_Size)
    {
        uint32_t Name = Get_B4();
        uint32_t Size = Get_L4();
        if (Name == 0x52494646) // "RIFF"
            Name = Get_B4();
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

    return 0;
}

//---------------------------------------------------------------------------
size_t riff::BitsPerBlock(riff::style Style)
{
    switch (Style)
    {
        case riff::PCM_48000_16_2_LE:   // 2x16-bit content
                                        return 32;
        case riff::PCM_48000_16_6_LE:   // 6x16-bit content
                                        return 96;
        case riff::PCM_48000_24_2_LE:   // 2x24-bit content
                                        return 48;
        case riff::PCM_48000_24_6_LE:   // 6x24-bit content
                                        return 144;
        default:
                                        return 0;
    }
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
    if (WriteFileCall)
    {
        rawcooked RAWcooked;
        RAWcooked.Unique = true;
        RAWcooked.WriteFileCall = WriteFileCall;
        RAWcooked.WriteFileCall_Opaque = WriteFileCall_Opaque;
        RAWcooked.Before = Buffer;
        RAWcooked.Before_Size = Buffer_Offset;
        RAWcooked.After = Buffer + Levels[Level].Offset_End;
        RAWcooked.After_Size = Buffer_Size - Levels[Level].Offset_End;
        RAWcooked.Parse();
    }
}

