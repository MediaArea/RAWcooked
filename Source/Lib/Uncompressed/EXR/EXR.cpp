/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Uncompressed/EXR/EXR.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
#include <sstream>
#include <ios>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace exr_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "file smaller than expected",
    "Version number",
    "Version flags",
    "Expected data size is bigger than real file size",
};

enum code : uint8_t
{
    BufferOverflow,
    VersionNumber,
    VersionFlags,
    DataSize,
    Max
};

} // unparsable

namespace unsupported
{

static const char* MessageText[] =
{
    // Unsupported
    "chList features (pLinear / reserved / xSampling / ySampling)",
    "compression",
    "dataWindow",
    "displayWindow",
    "framesPerSecond",
    "imageRotation",
    "lineOrder",
    "screenWindowCenter",
    "screenWindowWidth",
    //"Encoding",
    "Header field name",
    "Flavor (colorSpace / pixelType combination)",
    "Internal error",
};

enum code : uint8_t
{
    chListFeatures,
    compression,
    dataWindow,
    displayWindow,
    framesPerSecond,
    imageRotation,
    lineOrder,
    screenWindowCenter,
    screenWindowWidth,
    //Encoding,
    FieldName,
    Flavor,
    InternalError,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unsupported

namespace invalid
{

static const char* MessageText[] =
{
    "Size of field name",
    "Size of chList",
    "Size of chList channel name",
};

enum code : uint8_t
{
    FieldNameSize,
    chListSize,
    chListChannelNameSize,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // invalid

const char** ErrorTexts[] =
{
    undecodable::MessageText,
    unsupported::MessageText,
    nullptr,
    invalid::MessageText,
};

static_assert(error::type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // exr_issue

using namespace exr_issue;

//---------------------------------------------------------------------------
// Enums
enum class colorspace : uint8_t
{
    RGB,
};

enum class pixeltype : uint8_t
{
    Uint,
    Half,
    Float,
};

static const char* TypeText[] =
{
    "box2i",
    "chlist",
    "chromaticities",
    "compression",
    "float",
    "lineOrder",
    "int",
    "string",
    "rational",
    "timecode",
    "v2f",
};

enum class type : uint8_t
{
    box2i,
    chlist,
    chromaticities,
    compression,
    Float,
    lineOrder,
    Int,
    String,
    rational,
    timecode,
    v2f,
    Max
};
static_assert((size_t)type::Max == sizeof(TypeText) / sizeof(const char*), IncoherencyMessage);

//---------------------------------------------------------------------------
// Tested cases
struct exr_tested
{
    colorspace                  ColorSpace;
    pixeltype                   pixelType;

    bool operator == (const exr_tested &Value) const
    {
        return ColorSpace == Value.ColorSpace
            && pixelType == Value.pixelType;
    }
};
struct exr_also
{
    exr_tested                  Test;
    exr::flavor                 Flavor;
};

struct exr_tested EXR_Tested[] =
{
    { colorspace::RGB      , pixeltype::Half },
};
static_assert(exr::flavor_Max == sizeof(EXR_Tested) / sizeof(exr_tested), IncoherencyMessage);

//***************************************************************************
// EXR
//***************************************************************************

//---------------------------------------------------------------------------
exr::exr(errors* Errors_Source) :
    input_base_uncompressed_video(Errors_Source, Parser_EXR, true)
{
}

//---------------------------------------------------------------------------
exr::~exr()
{
}

//---------------------------------------------------------------------------
void exr::ParseBuffer()
{
    // Test that it is a EXR
    if (Buffer.Size() < 8)
        return;

    Buffer_Offset = 0;
    uint32_t MagicNumber = Get_B4();
    if (MagicNumber != 0x762F3101)
        return;
    SetDetected();

    uint32_t Version = Get_B4();
    switch (Version >> 24)
    {
    case 0x02:
        break;
    default:
        Undecodable(undecodable::VersionNumber);
        return;
    }
    bool LongName = false;
    if (Version&0xFFFFFF)
    {
        Undecodable(undecodable::VersionFlags);
        return;
    }

    IsBigEndian = false;
    exr_tested Info;
    Info.ColorSpace = (colorspace)-1;
    Info.pixelType = (pixeltype)-1;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t displayWidth = 0;
    uint32_t displayHeight = 0;
    bool displayIsPresent = false;
    bool UnsupportedFieldName = false;
    while (Buffer_Offset < Buffer.Size())
    {
        //Name
        size_t name_End = 0;
        while (name_End < Buffer.Size() - Buffer_Offset)
        {
            if (!Buffer[Buffer_Offset + name_End])
                break;
            if (name_End > (LongName ? 255 : 31))
                break;
            name_End++;
        }
        if (name_End >= Buffer.Size() - Buffer_Offset)
        {
            Invalid(invalid::FieldNameSize);
            return;
        }
        if (name_End > (LongName ? 255 : 31))
        {
            Invalid(invalid::FieldNameSize);
            return;
        }
        if (!name_End)
        {
            Buffer_Offset++;
            break; // Now it is image data
        }

        //Type
        size_t type_End = 0;
        while (name_End + 1 + type_End < Buffer.Size() - Buffer_Offset)
        {
            if (!Buffer[Buffer_Offset + name_End + 1 + type_End])
                break;
            if (type_End > (LongName ? 255 : 31))
                break;
            type_End++;
        }
        if (name_End + 1 + type_End > Buffer.Size() - Buffer_Offset)
        {
            Invalid(invalid::FieldNameSize);
            return;
        }
        if (type_End > (LongName ? 255 : 31) || name_End + 1 + type_End + 1 + 4 >= Buffer.Size() - Buffer_Offset)
        {
            Invalid(invalid::FieldNameSize);
            return;
        }

        // Size
        Buffer_Offset += name_End + 1 + type_End + 1;
        uint32_t Size = Get_L4();
        if (Size > Buffer.Size() - Buffer_Offset)
        {
            Invalid(invalid::FieldNameSize);
            return;
        }

        // Parse
        auto name = (const char*)Buffer.Data() + Buffer_Offset - (name_End + 1 + type_End + 1 + 4);
        auto type = (const char*)Buffer.Data() + Buffer_Offset - (               type_End + 1 + 4);
        #define CASE_F(_NAME,_TYPE) else if (!strcmp(_NAME, name) && !strcmp(TypeText[(int)_TYPE], type))  // Full parsing needed
        #define CASE_S(_NAME,_TYPE) else if (!strcmp(_NAME, name) && !strcmp(TypeText[(int)_TYPE], type)){Buffer_Offset += Size;} // Supported and skipped
        #define CASE_S_STARTWITH(_NAME) else if (!strncmp(_NAME, name, strlen(_NAME))){Buffer_Offset += Size;} // Supported and skipped
        if (false);
        CASE_S("acesImageContainerFlag", type::Int)
        CASE_S("adoptedNeutral", type::v2f)
        CASE_S_STARTWITH("arri.")
        CASE_S_STARTWITH("camera")
        CASE_S("capDate", type::String)
        CASE_F("captureRate", type::rational)
        {
            if (Size != 8)
            {
                Unsupported(unsupported::framesPerSecond);
                Buffer_Offset += Size;
            }
            else
            {
                auto FrameRate_N = Get_L4();
                auto FrameRate_D = Get_L4();
                if (FrameRate_N && FrameRate_D && InputInfo && !InputInfo->FrameRate)
                    InputInfo->FrameRate = ((decltype(InputInfo->FrameRate))FrameRate_N) / FrameRate_D;
            }
        }
        CASE_F("channels", type::chlist)
        {
            if (!Size)
            {
                Invalid(invalid::chListSize); // Should finish with a single null byte
                return;
            }
            uint32_t ColorSpace = 0;
            uint8_t Count = 0;
            size_t End = Buffer_Offset + Size - 1;
            while (17 <= End - Buffer_Offset) // channelName ending null + pixelType + Others
            {
                // channelName
                size_t channelName_End = 0;
                while (channelName_End < Buffer.Size() - Buffer_Offset && channelName_End <= 255)
                {
                    if (!Buffer[Buffer_Offset + channelName_End] || channelName_End > 255)
                        break;
                    channelName_End++;
                }
                if (channelName_End > End - Buffer_Offset - 17)
                {
                    Invalid(invalid::chListSize);
                    return;
                }
                if (!channelName_End || channelName_End > 255)
                {
                    Invalid(invalid::chListChannelNameSize);
                    return;
                }
                if (Count > 3 || channelName_End != 1)
                    ColorSpace = (uint32_t)-1;
                else
                    ColorSpace |= Buffer[Buffer_Offset] << (Count * 8);
                Buffer_Offset += channelName_End + 1;

                // pixelType
                auto pixelTypeValue = Get_L4();
                auto pixelType = (pixelTypeValue >= (1 << sizeof(Info.pixelType))) ? (pixeltype)-1 : (pixeltype)pixelTypeValue;
                if (!Count)
                    Info.pixelType = pixelType; // Storing the first value
                else if (pixelType != Info.pixelType)// Testing that all but first values are same
                    Info.pixelType = (pixeltype)-1;

                // Others
                if (Get_L4() || Get_L4() != 1 || Get_L4() != 1)
                {
                    Unsupported(unsupported::chListFeatures); // pLinear / reserved / xSampling / ySampling
                    return;
                }

                Count++;
            }
            if (End - Buffer_Offset || Get_L1())
            {
                Invalid(invalid::chListSize); // Should finish with a single null byte
                return;
            }

            switch (ColorSpace)
            {
                case 0x00524742: Info.ColorSpace = colorspace::RGB; break;
                default:;
            }

        }
        CASE_S("chromaticities", type::chromaticities)
        CASE_S_STARTWITH("com.arri.")
        CASE_S("comments", type::String)
        CASE_F("compression", type::compression)
        {
            if (Size != 1)
            {
                Unsupported(unsupported::compression);
                Buffer_Offset += Size;
            }
            else
            {
                if (Get_L1())
                    Unsupported(unsupported::compression);
            }
        }
        CASE_F("dataWindow", type::box2i)
        {
            if (Size != 16)
            {
                Unsupported(unsupported::dataWindow);
                Buffer_Offset += Size;
            }
            else
            {
                if (Get_L4() || Get_L4())
                    Unsupported(unsupported::dataWindow);
                Width = Get_L4();
                Height = Get_L4();
                if (!Width || !Height)
                    Unsupported(unsupported::dataWindow);
            }
        }
        CASE_F("displayWindow", type::box2i)
        {
            if (Size != 16)
            {
                Unsupported(unsupported::displayWindow);
                Buffer_Offset += Size;
            }
            else
            {
                if (Get_L4() || Get_L4())
                    Unsupported(unsupported::displayWindow);
                displayWidth = Get_L4();
                displayHeight = Get_L4();
                if (!displayWidth || !displayHeight)
                    Unsupported(unsupported::displayWindow);
                else
                    displayIsPresent = true;
            }
        }
        CASE_S("expTime", type::Float)
        CASE_S("focalLength", type::Float)
        CASE_S("focus", type::Float)
        CASE_F("framesPerSecond", type::rational)
        {
            if (Size != 8)
            {
                Unsupported(unsupported::framesPerSecond);
                Buffer_Offset += Size;
            }
            else
            {
                auto FrameRate_N = Get_L4();
                auto FrameRate_D = Get_L4();
                if (!FrameRate_N || !FrameRate_D)
                    Unsupported(unsupported::framesPerSecond);
                else if (InputInfo)
                    InputInfo->FrameRate = ((decltype(InputInfo->FrameRate))FrameRate_N) / FrameRate_D;
            }
        }
        CASE_S("imageCounter", type::Int)
        CASE_F("imageRotation", type::Float)
        {
            if (Size != 4)
            {
                Unsupported(unsupported::imageRotation);
                Buffer_Offset += Size;
            }
            else
            {
                if (Get_L4())
                    Unsupported(unsupported::imageRotation);
            }
        }
        CASE_S_STARTWITH("interim.")
        CASE_S("isoSpeed", type::Float)
        CASE_S("lensMake", type::String)
        CASE_S("lensSerialNumber", type::String)
        CASE_F("lineOrder", type::lineOrder)
        {
            if (Size != 1)
            {
                Unsupported(unsupported::lineOrder);
                Buffer_Offset += Size;
            }
            else
            {
                if (Get_L1())
                    Unsupported(unsupported::lineOrder);
            }
        }
        CASE_S("originalImageFlag", type::Int)
        CASE_S("owner", type::String)
        CASE_S("pixelAspectRatio", type::Float)
        CASE_S("reelName", type::String)
        CASE_S("recorderFirmwareVersion", type::String)
        CASE_S("recorderMake", type::String)
        CASE_S("recorderModel", type::String)
        CASE_S("reelName", type::String)
        CASE_F("screenWindowCenter", type::v2f)
        {
            if (Size != 8)
            {
                Unsupported(unsupported::screenWindowCenter);
                Buffer_Offset += Size;
            }
            else
            {
                if (Get_L8())
                    Unsupported(unsupported::screenWindowCenter);
            }
        }
        CASE_F("screenWindowWidth", type::Float)
        {
            if (Size != 4)
            {
                Unsupported(unsupported::screenWindowWidth);
                Buffer_Offset += Size;
            }
            else
            {
                if (Get_XF4() != 1)
                    Unsupported(unsupported::screenWindowWidth);
            }
        }
        CASE_S("storageMediaSerialNumber", type::String)
        CASE_S("timeCode", type::timecode)
        CASE_S("timecodeRate", type::Int)
        else
        {
            UnsupportedFieldName = true;
            Buffer_Offset += Size;
        }
    }

    // Supported?
    if (displayIsPresent && (Width != displayWidth || Height != displayHeight))
        Unsupported(unsupported::displayWindow);
    Width++;
    Height++;
    if (UnsupportedFieldName)
        Unsupported(unsupported::FieldName);
    for (const auto& EXR_Tested_Item : EXR_Tested)
    {
        if (EXR_Tested_Item == Info)
        {
            Flavor = (decltype(Flavor))(&EXR_Tested_Item - EXR_Tested);
            break;
        }
    }
    if (Flavor == (decltype(Flavor))-1)
        Unsupported(unsupported::Flavor);
    if (HasErrors())
        return;

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
    if (true) //Info.BitDepth > 10)
        slice_x = slice_x * 3 / 2; // 1.5x more slices if 16-bit
    if (slice_x > Width / 2)
        slice_x = Width / 2;
    if (slice_x > Height / 2)
        slice_x = Height / 2;
    if (!slice_x)
        slice_x = 1;

    // Computing which slice count is suitable
    slice_y = slice_x;

    // Offset Tables
    Buffer_Offset += 8 * Height;
    
    // Computing OffsetAfterData
    size_t ContentSize_Multiplier = BytesPerBlock((flavor)Flavor);
    size_t OffsetAfterData = Buffer_Offset + (ContentSize_Multiplier * Width + 8) * Height;
    if (OffsetAfterData > Buffer.Size())
    {
        if (!Actions[Action_AcceptTruncated])
            Undecodable(undecodable::DataSize);
    }

    // Can we compress?
    if (!HasErrors())
        SetSupported();

    // Write RAWcooked file
    if (IsSupported() && RAWcooked)
    {
        RAWcooked->Unique = false;
        RAWcooked->BeforeData = Buffer.Data();
        RAWcooked->BeforeData_Size = Buffer_Offset;
        RAWcooked->AfterData = Buffer.Data() + OffsetAfterData;
        RAWcooked->AfterData_Size = Buffer.Size() - OffsetAfterData;
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
void exr::BufferOverflow()
{
    Undecodable(undecodable::BufferOverflow);
}

//---------------------------------------------------------------------------
string exr::Flavor_String()
{
    return EXR_Flavor_String(Flavor);
}

//---------------------------------------------------------------------------
size_t exr::BytesPerBlock(exr::flavor /*Flavor*/)
{
    return 6;
}

//---------------------------------------------------------------------------
size_t exr::PixelsPerBlock(exr::flavor /*Flavor*/)
{
    return 1;
}

//---------------------------------------------------------------------------
static colorspace ColorSpace(exr::flavor Flavor)
{
    return EXR_Tested[(size_t)Flavor].ColorSpace;
}
static const char* ColorSpace_String(exr::flavor Flavor)
{
    switch (ColorSpace(Flavor))
    {
    case colorspace::RGB : return "RGB";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
static uint8_t BitDepth(exr::flavor /*Flavor*/)
{
    return 16;
}
static const char* BitDepth_String(exr::flavor /*Flavor*/)
{
    return "16";
}

//---------------------------------------------------------------------------
static const char* PixelType_String(exr::flavor Flavor)
{
    switch (EXR_Tested[(size_t)Flavor].pixelType)
    {
    case pixeltype::Float : return "Float";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
string EXR_Flavor_String(uint8_t Flavor)
{
    string ToReturn("EXR/Raw/");
    ToReturn += ColorSpace_String((exr::flavor)Flavor);
    ToReturn += '/';
    ToReturn += BitDepth_String((exr::flavor)Flavor);
    ToReturn += "bit";
    const char* Value = PixelType_String((exr::flavor)Flavor);
    if (Value && Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    return ToReturn;
}
