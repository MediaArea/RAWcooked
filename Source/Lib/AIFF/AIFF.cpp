/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/AIFF/AIFF.h"
#include "Lib/RAWcooked/RAWcooked.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Tested cases
struct aiff_tested
{
    long double                 sampleRate;
    uint16_t                    sampleSize;
    uint16_t                    numChannels;
    aiff::endianess             Endianess;
};

const size_t AIFF_Tested_Size = 45;
struct aiff_tested AIFF_Tested[AIFF_Tested_Size] =
{
    { 44100,  8, 1, aiff::BE },
    { 44100,  8, 1, aiff::LE },
    { 44100,  8, 2, aiff::BE },
    { 44100,  8, 2, aiff::LE },
    { 44100,  8, 6, aiff::BE },
    { 44100,  8, 6, aiff::LE },
    { 44100, 16, 1, aiff::BE },
    { 44100, 16, 1, aiff::LE },
    { 44100, 16, 2, aiff::BE },
    { 44100, 16, 2, aiff::LE },
    { 44100, 16, 6, aiff::BE },
    { 44100, 16, 6, aiff::LE },
    { 44100, 24, 1, aiff::BE },
    { 44100, 24, 2, aiff::BE },
    { 44100, 24, 6, aiff::BE },
    { 48000,  8, 1, aiff::BE },
    { 48000,  8, 1, aiff::LE },
    { 48000,  8, 2, aiff::BE },
    { 48000,  8, 2, aiff::LE },
    { 48000,  8, 6, aiff::BE },
    { 48000,  8, 6, aiff::LE },
    { 48000, 16, 1, aiff::BE },
    { 48000, 16, 1, aiff::LE },
    { 48000, 16, 2, aiff::BE },
    { 48000, 16, 2, aiff::LE },
    { 48000, 16, 6, aiff::BE },
    { 48000, 16, 6, aiff::LE },
    { 48000, 24, 1, aiff::BE },
    { 48000, 24, 2, aiff::BE },
    { 48000, 24, 6, aiff::BE },
    { 96000,  8, 1, aiff::BE },
    { 96000,  8, 1, aiff::LE },
    { 96000,  8, 2, aiff::BE },
    { 96000,  8, 2, aiff::LE },
    { 96000,  8, 6, aiff::BE },
    { 96000,  8, 6, aiff::LE },
    { 96000, 16, 1, aiff::BE },
    { 96000, 16, 1, aiff::LE },
    { 96000, 16, 2, aiff::BE },
    { 96000, 16, 2, aiff::LE },
    { 96000, 16, 6, aiff::BE },
    { 96000, 16, 6, aiff::LE },
    { 96000, 24, 1, aiff::BE },
    { 96000, 24, 2, aiff::BE },
    { 96000, 24, 6, aiff::BE },
};

//---------------------------------------------------------------------------


#define ELEMENT_BEGIN(_VALUE) \
aiff::call aiff::SubElements_##_VALUE(uint64_t Name) \
{ \
    switch (Name) \
    { \

#define ELEMENT_CASE(_VALUE,_NAME) \
    case 0x##_VALUE:  Levels[Level].SubElements = &aiff::SubElements_##_NAME;  return &aiff::_NAME;

#define ELEMENT_VOID(_VALUE,_NAME) \
    case 0x##_VALUE:  Levels[Level].SubElements = &aiff::SubElements_Void;  return &aiff::_NAME;


#define ELEMENT_END() \
    default:                        return SubElements_Void(Name); \
    } \
} \

ELEMENT_BEGIN(_)
ELEMENT_CASE(41494643, AIFF)
ELEMENT_CASE(41494646, AIFF)
ELEMENT_END()

ELEMENT_BEGIN(AIFC)
ELEMENT_VOID(434F4D4D, AIFF_COMM)
ELEMENT_VOID(53534E44, AIFF_SSND)
ELEMENT_END()

ELEMENT_BEGIN(AIFF)
ELEMENT_VOID(434F4D4D, AIFF_COMM)
ELEMENT_VOID(53534E44, AIFF_SSND)
ELEMENT_END()


//---------------------------------------------------------------------------
aiff::call aiff::SubElements_Void(uint64_t Name)
{
    Levels[Level].SubElements = &aiff::SubElements_Void; return &aiff::Void;
}

//***************************************************************************
// AIFF
//***************************************************************************

//---------------------------------------------------------------------------
aiff::aiff() :
    input_base_uncompressed(Parser_AIFF)
{
}

//---------------------------------------------------------------------------
bool aiff::ParseBuffer()
{
    if (Buffer_Size < 12)
        return false;
    if (Buffer[0] != 'F' || Buffer[1] != 'O' || Buffer[2] != 'R' || Buffer[3] != 'M'
     || Buffer[8] != 'A' || Buffer[9] != 'I' || Buffer[10] != 'F' || (Buffer[11] != 'F' && Buffer[11] != 'C'))
        return false;
    IsDetected = true;

    Buffer_Offset = 0;
    Levels[0].Offset_End = Buffer_Size;
    Levels[0].SubElements = &aiff::SubElements__;
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
            Size = Get_B4();
            if (Name == 0x464F524D) // "FORM"
            {
                if (Size < 4)
                    return Unsupported("Incoherency detected while parsing AIFF");
                Name = Get_B4();
                Size -= 4;
            }
            if (Buffer_Offset + Size > Levels[Level - 1].Offset_End)
            {
                // Truncated
                if (!AcceptTruncated)
                    return Unsupported("Truncated AIFF?");
                Size = Levels[Level - 1].Offset_End - Buffer_Offset;
            }
        }
        else
            return Unsupported("Incoherency detected while parsing AIFF");

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

    if (Flavor == (uint8_t)-1)
    {
        Unsupported("Flavor (sampleRate / sampleSize / numChannels / Endianess combination)");
        return false;
    }

    return ErrorMessage() ? true : false;
}

//---------------------------------------------------------------------------
string aiff::Flavor_String()
{
    return AIFF_Flavor_String(Flavor);
}

//---------------------------------------------------------------------------
uint16_t aiff::sampleSize()
{
    return AIFF_Tested[Flavor].sampleSize;
}

//---------------------------------------------------------------------------
aiff::endianess aiff::Endianess()
{
    return AIFF_Tested[Flavor].Endianess;
}

//---------------------------------------------------------------------------
void aiff::Void()
{
}

//---------------------------------------------------------------------------
void aiff::AIFC()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void aiff::AIFF()
{
    IsList = true;
}

//---------------------------------------------------------------------------
void aiff::AIFF_COMM()
{
    if (Levels[Level].Offset_End - Buffer_Offset < 16)
    {
        Unsupported("AIFF FormatTag format");
        return;
    }

    uint16_t numChannels = Get_B2();
    uint32_t numSamplesFrames = Get_B4();
    uint16_t sampleSize = Get_B2();
    long double sampleRate = Get_BF10();
    endianess Endianess = BE;
    
    if (Levels[Level].Offset_End - Buffer_Offset)
    {
        uint32_t compressionType = Get_B4();
        switch (compressionType)
        {
            case 0x72617720:
            case 0x736F7774 :
                                Endianess = LE;
                                break;
        }
    }

    // Supported?
    uint8_t Tested = 0;
    for (; Tested < AIFF_Tested_Size; Tested++)
    {
        aiff_tested& AIFF_Tested_Item = AIFF_Tested[Tested];
        if (AIFF_Tested_Item.sampleRate == sampleRate
            && AIFF_Tested_Item.sampleSize == sampleSize
            && AIFF_Tested_Item.numChannels == numChannels
            && AIFF_Tested_Item.Endianess == Endianess)
            break;
    }
    if (Tested >= AIFF_Tested_Size)
    {
        Unsupported("Flavor (sampleRate / sampleSize / numChannels / Endianess combination)");
        return;
    }
    Flavor = Tested;
}

//---------------------------------------------------------------------------
void aiff::AIFF_SSND()
{
    // Write RAWcooked file
    if (RAWcooked)
    {
        RAWcooked->Unique = true;
        RAWcooked->Before = Buffer;
        RAWcooked->Before_Size = Buffer_Offset + 8;
        RAWcooked->After = Buffer + Levels[Level].Offset_End;
        RAWcooked->After_Size = Buffer_Size - Levels[Level].Offset_End;
        RAWcooked->Parse();
    }
}

//---------------------------------------------------------------------------
uint32_t aiff::sampleRate(aiff::flavor Flavor)
{
    switch (Flavor)
    {
        case PCM_44100_8_1_S:
        case PCM_44100_8_1_U:
        case PCM_44100_8_2_S:
        case PCM_44100_8_2_U:
        case PCM_44100_8_6_S:
        case PCM_44100_8_6_U:
        case PCM_44100_16_1_BE:
        case PCM_44100_16_1_LE:
        case PCM_44100_16_2_BE:
        case PCM_44100_16_2_LE:
        case PCM_44100_16_6_BE:
        case PCM_44100_16_6_LE:
        case PCM_44100_24_1_BE:
        case PCM_44100_24_2_BE:
        case PCM_44100_24_6_BE:
                                        return 44100;
        case PCM_48000_8_1_S:
        case PCM_48000_8_1_U:
        case PCM_48000_8_2_S:
        case PCM_48000_8_2_U:
        case PCM_48000_8_6_S:
        case PCM_48000_8_6_U:
        case PCM_48000_16_1_BE:
        case PCM_48000_16_1_LE:
        case PCM_48000_16_2_BE:
        case PCM_48000_16_2_LE:
        case PCM_48000_16_6_BE:
        case PCM_48000_16_6_LE:
        case PCM_48000_24_1_BE:
        case PCM_48000_24_2_BE:
        case PCM_48000_24_6_BE:
                                        return 48000;
        case PCM_96000_8_1_S:
        case PCM_96000_8_1_U:
        case PCM_96000_8_2_S:
        case PCM_96000_8_2_U:
        case PCM_96000_8_6_S:
        case PCM_96000_8_6_U:
        case PCM_96000_16_1_BE:
        case PCM_96000_16_1_LE:
        case PCM_96000_16_2_BE:
        case PCM_96000_16_2_LE:
        case PCM_96000_16_6_BE:
        case PCM_96000_16_6_LE:
        case PCM_96000_24_1_BE:
        case PCM_96000_24_2_BE:
        case PCM_96000_24_6_BE:
                                        return 96000;
        default:
                                        return 0;
    }
}
const char* aiff::sampleRate_String(aiff::flavor Flavor)
{
    uint32_t Value = aiff::sampleRate(Flavor);

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
uint16_t aiff::sampleSize(aiff::flavor Flavor)
{
    switch (Flavor)
    {
        case PCM_44100_8_1_S:
        case PCM_44100_8_1_U:
        case PCM_44100_8_2_S:
        case PCM_44100_8_2_U:
        case PCM_44100_8_6_S:
        case PCM_44100_8_6_U:
        case PCM_48000_8_1_S:
        case PCM_48000_8_1_U:
        case PCM_48000_8_2_S:
        case PCM_48000_8_2_U:
        case PCM_48000_8_6_S:
        case PCM_48000_8_6_U:
        case PCM_96000_8_1_S:
        case PCM_96000_8_1_U:
        case PCM_96000_8_2_S:
        case PCM_96000_8_2_U:
        case PCM_96000_8_6_S:
        case PCM_96000_8_6_U:
                                        return 8;
        case PCM_44100_16_1_BE:
        case PCM_44100_16_1_LE:
        case PCM_44100_16_2_BE:
        case PCM_44100_16_2_LE:
        case PCM_44100_16_6_BE:
        case PCM_44100_16_6_LE:
        case PCM_48000_16_1_BE:
        case PCM_48000_16_1_LE:
        case PCM_48000_16_2_BE:
        case PCM_48000_16_2_LE:
        case PCM_48000_16_6_BE:
        case PCM_48000_16_6_LE:
        case PCM_96000_16_1_BE:
        case PCM_96000_16_1_LE:
        case PCM_96000_16_2_BE:
        case PCM_96000_16_2_LE:
        case PCM_96000_16_6_BE:
        case PCM_96000_16_6_LE:
                                        return 16;
        case PCM_44100_24_1_BE:
        case PCM_44100_24_2_BE:
        case PCM_44100_24_6_BE:
        case PCM_48000_24_1_BE:
        case PCM_48000_24_2_BE:
        case PCM_48000_24_6_BE:
        case PCM_96000_24_1_BE:
        case PCM_96000_24_2_BE:
        case PCM_96000_24_6_BE:
                                        return 24;
        default:
                                        return 0;
    }
}
const char* aiff::sampleSize_String(aiff::flavor Flavor)
{
    uint16_t Value = aiff::sampleSize(Flavor);

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
uint8_t aiff::numChannels(aiff::flavor Flavor)
{
    switch (Flavor)
    {
        case PCM_44100_8_1_S:
        case PCM_44100_8_1_U:
        case PCM_48000_8_1_S:
        case PCM_48000_8_1_U:
        case PCM_96000_8_1_S:
        case PCM_96000_8_1_U:
        case PCM_44100_16_1_BE:
        case PCM_44100_16_1_LE:
        case PCM_48000_16_1_BE:
        case PCM_48000_16_1_LE:
        case PCM_96000_16_1_BE:
        case PCM_96000_16_1_LE:
        case PCM_48000_24_1_BE:
        case PCM_44100_24_1_BE:
        case PCM_96000_24_1_BE:
                                        return 1;
        case PCM_44100_8_2_S:
        case PCM_44100_8_2_U:
        case PCM_48000_8_2_S:
        case PCM_48000_8_2_U:
        case PCM_96000_8_2_S:
        case PCM_96000_8_2_U:
        case PCM_44100_16_2_BE:
        case PCM_44100_16_2_LE:
        case PCM_48000_16_2_BE:
        case PCM_48000_16_2_LE:
        case PCM_96000_16_2_BE:
        case PCM_96000_16_2_LE:
        case PCM_44100_24_2_BE:
        case PCM_48000_24_2_BE:
        case PCM_96000_24_2_BE:
                                        return 2;
        case PCM_44100_8_6_S:
        case PCM_44100_8_6_U:
        case PCM_48000_8_6_S:
        case PCM_48000_8_6_U:
        case PCM_96000_8_6_S:
        case PCM_96000_8_6_U:
        case PCM_44100_16_6_BE:
        case PCM_44100_16_6_LE:
        case PCM_48000_16_6_BE:
        case PCM_48000_16_6_LE:
        case PCM_96000_16_6_BE:
        case PCM_96000_16_6_LE:
        case PCM_44100_24_6_BE:
        case PCM_48000_24_6_BE:
        case PCM_96000_24_6_BE:
                                        return 6;
        default:
                                        return 0;
    }
}
const char* aiff::numChannels_String(aiff::flavor Flavor)
{
    uint8_t Value = aiff::numChannels(Flavor);

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
aiff::endianess aiff::Endianess(aiff::flavor Flavor)
{
    switch (Flavor)
    {
        case PCM_44100_8_1_S:
        case PCM_44100_8_2_S:
        case PCM_44100_8_6_S:
        case PCM_48000_8_1_S:
        case PCM_48000_8_2_S:
        case PCM_48000_8_6_S:
        case PCM_96000_8_1_S:
        case PCM_96000_8_2_S:
        case PCM_96000_8_6_S:
        case PCM_44100_16_1_BE:
        case PCM_44100_16_2_BE:
        case PCM_44100_16_6_BE:
        case PCM_48000_16_1_BE:
        case PCM_48000_16_2_BE:
        case PCM_48000_16_6_BE:
        case PCM_96000_16_1_BE:
        case PCM_96000_16_2_BE:
        case PCM_96000_16_6_BE:
        case PCM_44100_24_1_BE:
        case PCM_44100_24_2_BE:
        case PCM_44100_24_6_BE:
        case PCM_48000_24_1_BE:
        case PCM_48000_24_2_BE:
        case PCM_48000_24_6_BE:
        case PCM_96000_24_1_BE:
        case PCM_96000_24_2_BE:
        case PCM_96000_24_6_BE:
                                        return BE; // Or Signed
        case PCM_44100_8_1_U:
        case PCM_44100_8_2_U:
        case PCM_44100_8_6_U:
        case PCM_48000_8_1_U:
        case PCM_48000_8_2_U:
        case PCM_48000_8_6_U:
        case PCM_96000_8_1_U:
        case PCM_96000_8_2_U:
        case PCM_96000_8_6_U:
        case PCM_44100_16_1_LE:
        case PCM_44100_16_2_LE:
        case PCM_44100_16_6_LE:
        case PCM_48000_16_1_LE:
        case PCM_48000_16_2_LE:
        case PCM_48000_16_6_LE:
        case PCM_96000_16_1_LE:
        case PCM_96000_16_2_LE:
        case PCM_96000_16_6_LE:
                                        return LE; // Or Unsigned
        default:
                                        return (endianess)-1;
    }
}
const char* aiff::Endianess_String(aiff::flavor Flavor)
{
    aiff::endianess Value = aiff::Endianess(Flavor);
    uint16_t sampleSize = aiff::sampleSize(Flavor);

    switch (Value)
    {
        case LE:
                                        return sampleSize==8?"U":"LE";
        case BE:
                                        return sampleSize==8?"S":"BE";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
string AIFF_Flavor_String(uint8_t Flavor)
{
    string ToReturn("AIFF/PCM/");
    ToReturn += aiff::sampleRate_String((aiff::flavor)Flavor);
    ToReturn += "kHz/";
    ToReturn += aiff::sampleSize_String((aiff::flavor)Flavor);
    ToReturn += "bit/";
    ToReturn += aiff::numChannels_String((aiff::flavor)Flavor);
    ToReturn += "ch";
    const char* Value = aiff::Endianess_String((aiff::flavor)Flavor);
    if (Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    return ToReturn;
}

