/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Uncompressed/AIFF/AIFF.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace aiff_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "file smaller than expected",
    "FORM chunk size",
    "COMM chunk size",
    "truncated chunk",
};

enum code : uint8_t
{
    BufferOverflow,
    FORM_ChunkSize,
    COMM_ChunkSize,
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
    "COMM compressionType not known PCM",
    "COMM chunk not before data chunk",
    "Flavor (sampleRate / sampleSize / numChannels / Endianness combination)",
};

enum code : uint8_t
{
    COMM_compressionType_NotPcm,
    COMM_Location,
    Flavor,
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

} // aiff_issue

using namespace aiff_issue;

//---------------------------------------------------------------------------
// Enums
enum class sample_rate_code : uint8_t
{
    _44100,
    _48000,
    _96000,
};
    
//---------------------------------------------------------------------------
// Tested cases
struct aiff_tested
{
    sample_rate_code            sampleRateCode;
    uint8_t                     sampleSize;
    uint8_t                     numChannels;
    endianness                  Endianness;

    bool operator == (const aiff_tested& Value) const
    {
        return sampleRateCode == Value.sampleRateCode
            && sampleSize == Value.sampleSize
            && numChannels == Value.numChannels
            && Endianness == Value.Endianness;
    }
};

struct aiff_tested AIFF_Tested[] =
{
    { sample_rate_code::_44100,  8, 1, endianness::LE },
    { sample_rate_code::_44100,  8, 1, endianness::BE },
    { sample_rate_code::_44100,  8, 2, endianness::LE },
    { sample_rate_code::_44100,  8, 2, endianness::BE },
    { sample_rate_code::_44100,  8, 6, endianness::LE },
    { sample_rate_code::_44100,  8, 6, endianness::BE },
    { sample_rate_code::_44100, 16, 1, endianness::LE },
    { sample_rate_code::_44100, 16, 1, endianness::BE },
    { sample_rate_code::_44100, 16, 2, endianness::LE },
    { sample_rate_code::_44100, 16, 2, endianness::BE },
    { sample_rate_code::_44100, 16, 6, endianness::LE },
    { sample_rate_code::_44100, 16, 6, endianness::BE },
    { sample_rate_code::_44100, 24, 1, endianness::BE },
    { sample_rate_code::_44100, 24, 2, endianness::BE },
    { sample_rate_code::_44100, 24, 6, endianness::BE },
    { sample_rate_code::_48000,  8, 1, endianness::LE },
    { sample_rate_code::_48000,  8, 1, endianness::BE },
    { sample_rate_code::_48000,  8, 2, endianness::LE },
    { sample_rate_code::_48000,  8, 2, endianness::BE },
    { sample_rate_code::_48000,  8, 6, endianness::LE },
    { sample_rate_code::_48000,  8, 6, endianness::BE },
    { sample_rate_code::_48000, 16, 1, endianness::LE },
    { sample_rate_code::_48000, 16, 1, endianness::BE },
    { sample_rate_code::_48000, 16, 2, endianness::LE },
    { sample_rate_code::_48000, 16, 2, endianness::BE },
    { sample_rate_code::_48000, 16, 6, endianness::LE },
    { sample_rate_code::_48000, 16, 6, endianness::BE },
    { sample_rate_code::_48000, 24, 1, endianness::BE },
    { sample_rate_code::_48000, 24, 2, endianness::BE },
    { sample_rate_code::_48000, 24, 6, endianness::BE },
    { sample_rate_code::_96000,  8, 1, endianness::LE },
    { sample_rate_code::_96000,  8, 1, endianness::BE },
    { sample_rate_code::_96000,  8, 2, endianness::LE },
    { sample_rate_code::_96000,  8, 2, endianness::BE },
    { sample_rate_code::_96000,  8, 6, endianness::BE },
    { sample_rate_code::_96000,  8, 6, endianness::LE },
    { sample_rate_code::_96000, 16, 1, endianness::LE },
    { sample_rate_code::_96000, 16, 1, endianness::BE },
    { sample_rate_code::_96000, 16, 2, endianness::LE },
    { sample_rate_code::_96000, 16, 2, endianness::BE },
    { sample_rate_code::_96000, 16, 6, endianness::LE },
    { sample_rate_code::_96000, 16, 6, endianness::BE },
    { sample_rate_code::_96000, 24, 1, endianness::BE },
    { sample_rate_code::_96000, 24, 2, endianness::BE },
    { sample_rate_code::_96000, 24, 6, endianness::BE },
};
static_assert(aiff::flavor_Max == sizeof(AIFF_Tested) / sizeof(aiff_tested), IncoherencyMessage);

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
ELEMENT_CASE(464F524D41494643, AIFF)
ELEMENT_CASE(464F524D41494646, AIFF)
ELEMENT_END()

ELEMENT_BEGIN(AIFF)
ELEMENT_VOID(434F4D4D, AIFF_COMM)
ELEMENT_VOID(53534E44, AIFF_SSND)
ELEMENT_END()


//---------------------------------------------------------------------------
aiff::call aiff::SubElements_Void(uint64_t /*Name*/)
{
    Levels[Level].SubElements = &aiff::SubElements_Void; return &aiff::Void;
}

//***************************************************************************
// AIFF
//***************************************************************************

//---------------------------------------------------------------------------
aiff::aiff(errors* Errors_Source) :
    input_base_uncompressed_audio(Errors_Source, Parser_AIFF)
{
}

//---------------------------------------------------------------------------
void aiff::ParseBuffer()
{
    if (Buffer.Size() < 12)
        return;
    if (Buffer[0] != 'F' || Buffer[1] != 'O' || Buffer[2] != 'R' || Buffer[3] != 'M'
     || Buffer[8] != 'A' || Buffer[9] != 'I' || Buffer[10] != 'F' || (Buffer[11] != 'F' && Buffer[11] != 'C'))
        return;
    SetDetected();

    Flavor = flavor_Max; // Used for detected if COMM chunk is parsed
    Buffer_Offset = 0;
    Levels[0].Offset_End = Buffer.Size();
    Levels[0].SubElements = &aiff::SubElements__;
    Level=1;

    while (Buffer_Offset < Buffer.Size())
    {
        // Find the current nesting level
        while (Buffer_Offset >= Levels[Level - 1].Offset_End)
        {
            Levels[Level].SubElements = nullptr;
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
        Size = Get_B4();
        if (Name == 0x464F524D) // "FORM"
        {
            if (Size < 4 || Buffer_Offset + 4 > End)
            {
                Undecodable(undecodable::FORM_ChunkSize);
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

    if (Flavor == (uint8_t)-1)
    {
        Unsupported(unsupported::Flavor);
        return;
    }
}

//---------------------------------------------------------------------------
void aiff::BufferOverflow()
{
    Undecodable(undecodable::BufferOverflow);
}

//---------------------------------------------------------------------------
string aiff::Flavor_String()
{
    return AIFF_Flavor_String(Flavor);
}

//---------------------------------------------------------------------------
uint8_t aiff::BitDepth()
{
    return AIFF_Tested[Flavor].sampleSize;
}

//---------------------------------------------------------------------------
endianness aiff::Endianness()
{
    return AIFF_Tested[Flavor].Endianness;
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
        Undecodable(undecodable::COMM_ChunkSize);
        return;
    }

    aiff_tested Info;
    uint16_t numChannels = Get_B2();
    Get_B4(); // numSamplesFrames
    uint16_t sampleSize = Get_B2();
    long double sampleRate = Get_BF10();
    Info.Endianness = endianness::BE;
    if (Levels[Level].Offset_End - Buffer_Offset)
    {
        uint32_t compressionType = Get_B4();
        switch (compressionType)
        {
        case 0x4E4F4E45: // NONE
        case 0x74776F73: // twos
            break;
        case 0x72617720: // raw
        case 0x736F7774: // sowt
            Info.Endianness = endianness::LE;
            break;
        default:
            Unsupported(unsupported::COMM_compressionType_NotPcm);
            return;
        }
    }

    // Supported?
    if (sampleRate < 0
        || sampleRate > (unsigned int)-1
        || (sampleRate - (int)sampleRate) // Fractional part
        || sampleSize > (decltype(aiff_tested::sampleSize))-1
        || numChannels > (decltype(aiff_tested::numChannels))-1)
    {
        Unsupported(unsupported::Flavor);
        return;
    }
    switch ((unsigned int)sampleRate)
    {
    case 44100: Info.sampleRateCode = sample_rate_code::_44100; break;
    case 48000: Info.sampleRateCode = sample_rate_code::_48000; break;
    case 96000: Info.sampleRateCode = sample_rate_code::_96000; break;
    default   : Info.sampleRateCode = (decltype(Info.sampleRateCode))-1;
    }
    Info.sampleSize = (decltype(aiff_tested::sampleSize))sampleSize;
    Info.numChannels = (decltype(aiff_tested::numChannels))numChannels;
    for (const auto& AIFF_Tested_Item : AIFF_Tested)
    {
        if (AIFF_Tested_Item == Info)
        {
            Flavor = (decltype(Flavor))(&AIFF_Tested_Item - AIFF_Tested);
            break;
        }
    }
    if (Flavor == (decltype(Flavor))-1)
        Unsupported(unsupported::Flavor);
    if (HasErrors())
        return;
}

//---------------------------------------------------------------------------
void aiff::AIFF_SSND()
{
    // Test if fmt chunk was parsed
    if (!HasErrors() && Flavor == flavor_Max)
        Unsupported(unsupported::COMM_Location);

    // Can we compress?
    if (!HasErrors())
        SetSupported();

    // Write RAWcooked file
    if (IsSupported() && RAWcooked)
    {
        RAWcooked->Unique = true;
        RAWcooked->BeforeData = Buffer.Data();
        RAWcooked->BeforeData_Size = Buffer_Offset + 8;
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
static const char* sampleRate_String(aiff::flavor Flavor)
{
    switch (AIFF_Tested[(size_t)Flavor].sampleRateCode)
    {
    case sample_rate_code::_44100: return "44";
    case sample_rate_code::_48000: return "48";
    case sample_rate_code::_96000: return "96";
    }
    return nullptr;
}

//---------------------------------------------------------------------------
static uint8_t sampleSize(aiff::flavor Flavor)
{
    return AIFF_Tested[(size_t)Flavor].sampleSize;
}
static string sampleSize_String(aiff::flavor Flavor)
{
    return to_string(AIFF_Tested[(size_t)Flavor].sampleSize);
}

//---------------------------------------------------------------------------
static string numChannels_String(aiff::flavor Flavor)
{
    return to_string(AIFF_Tested[(size_t)Flavor].numChannels);
}

//---------------------------------------------------------------------------
static endianness Endianness(aiff::flavor Flavor)
{
    return AIFF_Tested[(size_t)Flavor].Endianness;
}
static const char* Endianess_String(aiff::flavor Flavor)
{
    switch (Endianness(Flavor))
    {
    case endianness::LE: return sampleSize(Flavor) == 8 ? "U" : "LE";
    case endianness::BE: return sampleSize(Flavor) == 8 ? "S" : "BE";
    }
    return nullptr;
}

//---------------------------------------------------------------------------
string AIFF_Flavor_String(uint8_t Flavor)
{
    string ToReturn("AIFF/PCM/");
    ToReturn += sampleRate_String((aiff::flavor)Flavor);
    ToReturn += "kHz/";
    ToReturn += sampleSize_String((aiff::flavor)Flavor);
    ToReturn += "bit/";
    ToReturn += numChannels_String((aiff::flavor)Flavor);
    ToReturn += "ch";
    const char* Value = Endianess_String((aiff::flavor)Flavor);
    if (Value)
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    return ToReturn;
}
