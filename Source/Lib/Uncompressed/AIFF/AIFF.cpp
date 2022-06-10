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
    "Flavor (sampleRate / sampleSize / numChannels / Sign / Endianness combination)",
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
// Tested cases
struct aiff_tested
{
    samplerate_code             sampleRateCode;
    bitdepth                    sampleSize;
    channels                    numChannels;
    sign                        Sign;
    endianness                  Endianness;

    bool operator == (const aiff_tested& Value) const
    {
        return sampleRateCode == Value.sampleRateCode
            && sampleSize == Value.sampleSize
            && numChannels == Value.numChannels
            && Sign == Value.Sign
            && Endianness == Value.Endianness
            ;
    }
};

struct aiff_tested AIFF_Tested[] =
{
    { samplerate_code::_44100,  8, 1, sign::U, endianness::LE},
    { samplerate_code::_44100,  8, 1, sign::S, endianness::LE},
    { samplerate_code::_44100,  8, 2, sign::U, endianness::LE},
    { samplerate_code::_44100,  8, 2, sign::S, endianness::LE},
    { samplerate_code::_44100,  8, 6, sign::U, endianness::LE},
    { samplerate_code::_44100,  8, 6, sign::S, endianness::LE},
    { samplerate_code::_44100,  8, 8, sign::U, endianness::LE},
    { samplerate_code::_44100,  8, 8, sign::S, endianness::LE},
    { samplerate_code::_44100, 16, 1, sign::S, endianness::LE},
    { samplerate_code::_44100, 16, 1, sign::S, endianness::BE},
    { samplerate_code::_44100, 16, 2, sign::S, endianness::LE},
    { samplerate_code::_44100, 16, 2, sign::S, endianness::BE},
    { samplerate_code::_44100, 16, 6, sign::S, endianness::LE},
    { samplerate_code::_44100, 16, 6, sign::S, endianness::BE},
    { samplerate_code::_44100, 16, 8, sign::S, endianness::LE},
    { samplerate_code::_44100, 16, 8, sign::S, endianness::BE},
    { samplerate_code::_44100, 24, 1, sign::S, endianness::BE},
    { samplerate_code::_44100, 24, 2, sign::S, endianness::BE},
    { samplerate_code::_44100, 24, 6, sign::S, endianness::BE},
    { samplerate_code::_44100, 24, 8, sign::S, endianness::BE},
    { samplerate_code::_44100, 32, 1, sign::S, endianness::BE},
    { samplerate_code::_44100, 32, 2, sign::S, endianness::BE},
    { samplerate_code::_44100, 32, 6, sign::S, endianness::BE},
    { samplerate_code::_44100, 32, 8, sign::S, endianness::BE},
    { samplerate_code::_48000,  8, 1, sign::U, endianness::LE},
    { samplerate_code::_48000,  8, 1, sign::S, endianness::LE},
    { samplerate_code::_48000,  8, 2, sign::U, endianness::LE},
    { samplerate_code::_48000,  8, 2, sign::S, endianness::LE},
    { samplerate_code::_48000,  8, 6, sign::U, endianness::LE},
    { samplerate_code::_48000,  8, 6, sign::S, endianness::LE},
    { samplerate_code::_48000,  8, 8, sign::U, endianness::LE},
    { samplerate_code::_48000,  8, 8, sign::S, endianness::LE},
    { samplerate_code::_48000, 16, 1, sign::S, endianness::LE},
    { samplerate_code::_48000, 16, 1, sign::S, endianness::BE},
    { samplerate_code::_48000, 16, 2, sign::S, endianness::LE},
    { samplerate_code::_48000, 16, 2, sign::S, endianness::BE},
    { samplerate_code::_48000, 16, 6, sign::S, endianness::LE},
    { samplerate_code::_48000, 16, 6, sign::S, endianness::BE},
    { samplerate_code::_48000, 16, 8, sign::S, endianness::LE},
    { samplerate_code::_48000, 16, 8, sign::S, endianness::BE},
    { samplerate_code::_48000, 24, 1, sign::S, endianness::BE},
    { samplerate_code::_48000, 24, 2, sign::S, endianness::BE},
    { samplerate_code::_48000, 24, 6, sign::S, endianness::BE},
    { samplerate_code::_48000, 24, 8, sign::S, endianness::BE},
    { samplerate_code::_48000, 32, 1, sign::S, endianness::BE},
    { samplerate_code::_48000, 32, 2, sign::S, endianness::BE},
    { samplerate_code::_48000, 32, 6, sign::S, endianness::BE},
    { samplerate_code::_48000, 32, 8, sign::S, endianness::BE},
    { samplerate_code::_96000,  8, 1, sign::U, endianness::LE},
    { samplerate_code::_96000,  8, 1, sign::S, endianness::LE},
    { samplerate_code::_96000,  8, 2, sign::U, endianness::LE},
    { samplerate_code::_96000,  8, 2, sign::S, endianness::LE},
    { samplerate_code::_96000,  8, 6, sign::S, endianness::LE},
    { samplerate_code::_96000,  8, 6, sign::U, endianness::LE},
    { samplerate_code::_96000,  8, 8, sign::S, endianness::LE},
    { samplerate_code::_96000,  8, 8, sign::U, endianness::LE},
    { samplerate_code::_96000, 16, 1, sign::S, endianness::LE},
    { samplerate_code::_96000, 16, 1, sign::S, endianness::BE},
    { samplerate_code::_96000, 16, 2, sign::S, endianness::LE},
    { samplerate_code::_96000, 16, 2, sign::S, endianness::BE},
    { samplerate_code::_96000, 16, 6, sign::S, endianness::LE},
    { samplerate_code::_96000, 16, 6, sign::S, endianness::BE},
    { samplerate_code::_96000, 16, 8, sign::S, endianness::LE},
    { samplerate_code::_96000, 16, 8, sign::S, endianness::BE},
    { samplerate_code::_96000, 24, 1, sign::S, endianness::BE},
    { samplerate_code::_96000, 24, 2, sign::S, endianness::BE},
    { samplerate_code::_96000, 24, 6, sign::S, endianness::BE},
    { samplerate_code::_96000, 24, 8, sign::S, endianness::BE},
    { samplerate_code::_96000, 32, 1, sign::S, endianness::BE},
    { samplerate_code::_96000, 32, 2, sign::S, endianness::BE},
    { samplerate_code::_96000, 32, 6, sign::S, endianness::BE},
    { samplerate_code::_96000, 32, 8, sign::S, endianness::BE},
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

    Flavor = (decltype(Flavor))-1;
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
sign aiff::Sign()
{
    return AIFF_Tested[Flavor].Sign;
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
    if (Levels[Level].Offset_End - Buffer_Offset)
    {
        uint32_t compressionType = Get_B4();
        switch (compressionType)
        {
        case 0x4E4F4E45: // NONE
        case 0x74776F73: // twos
            Info.Endianness = endianness::BE;
            Info.Sign = sign::S;
            break;
        case 0x72617720: // raw
        case 0x736F7774: // sowt
            Info.Endianness = endianness::LE;
            Info.Sign = sampleSize > 8 ? sign::S : sign::U;
            break;
        default:
            Unsupported(unsupported::COMM_compressionType_NotPcm);
            return;
        }
    }
    else
    {
        Info.Sign = sign::S;
        Info.Endianness = sampleSize > 8 ? endianness::BE : endianness::LE;
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
    Info.sampleSize = (decltype(aiff_tested::sampleSize))sampleSize;
    Info.numChannels = (decltype(aiff_tested::numChannels))numChannels;
    Info.sampleRateCode = SampleRate2Code((uint32_t)sampleRate);
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
    if (!HasErrors() && Flavor == (decltype(Flavor))-1)
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
string AIFF_Flavor_String(uint8_t Flavor)
{
    const auto& Info = AIFF_Tested[(size_t)Flavor];
    string ToReturn("AIFF/");
    ToReturn += PCM_Flavor_String(Info.sampleSize, Info.Sign, Info.Endianness, Info.numChannels, Info.sampleRateCode);
    return ToReturn;
}
