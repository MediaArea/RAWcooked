/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
#include <sstream>
#include <ios>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace dpx_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "file smaller than expected",
    "Version number of header format",
    "Offset to data",
    "Expected data size is bigger than real file size",
};

enum code : uint8_t
{
    BufferOverflow,
    VersionNumber,
    OffsetToData,
    DataSize,
    Max
};

} // unparsable

namespace unsupported
{

static const char* MessageText[] =
{
    // Unsupported
    "Offset to image data in bytes",
    "Encryption key",
    "Image orientation",
    "Number of image elements",
    "Data sign",
    "Encoding",
    "End-of-line padding",
    "\"Frame rate of original (frames/s)\" not same as \"Temporal sampling rate or frame rate (Hz)\"",
    "\"Frame rate of original (frames/s)\" or \"Temporal sampling rate or frame rate (Hz)\" not present",
    "Flavor (Descriptor / BitDepth / Packing / Endianness combination)",
    "Pixels in slice not on a 32-bit boundary",
    "Internal error",
};

enum code : uint8_t
{
    OffsetToImageData,
    Encryption,
    Orientation,
    NumberOfElements,
    DataSign,
    Encoding,
    EolPadding,
    FrameRate_Incoherent,
    FrameRate_Unavailable,
    Flavor,
    PixelBoundaries,
    InternalError,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unsupported

namespace invalid
{

static const char* MessageText[] =
{
    "Offset to image data in bytes",
    "Total image file size",
    "Version number of header format",
    "Ditto key",
    "Ditto key is set to \"same as the previous frame\" but header data differs",
    "Number of image elements",
};

enum code : uint8_t
{
    OffsetToImageData,
    TotalImageFileSize,
    VersionNumber,
    DittoKey,
    DittoKey_NotSame,
    NumberOfElements,
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

} // dpx_issue

using namespace dpx_issue;

//---------------------------------------------------------------------------
// Enums
enum class colorspace : uint8_t
{
    RGB,
    RGBA,
    Y,
};

enum class packing : uint8_t
{
    Packed,
    MethodA,
    MethodB,
};

//---------------------------------------------------------------------------
// Tested cases
struct dpx_tested
{
    colorspace                  ColorSpace;
    uint8_t                     BitDepth;
    packing                     Packing;
    endianness                  Endianness;

    bool operator == (const dpx_tested &Value) const
    {
        return ColorSpace == Value.ColorSpace
            && BitDepth == Value.BitDepth
            && Packing == Value.Packing
            && Endianness == Value.Endianness;
    }
};
struct dpx_also
{
    dpx_tested                    Test;
    dpx::flavor                 Flavor;
};

struct dpx_tested DPX_Tested[] =
{
    { colorspace::RGB      ,  8, packing::Packed , endianness::LE },
    { colorspace::RGB      , 10, packing::MethodA, endianness::LE },
    { colorspace::RGB      , 10, packing::MethodA, endianness::BE },
    { colorspace::RGB      , 12, packing::Packed , endianness::BE },
    { colorspace::RGB      , 12, packing::MethodA, endianness::LE },
    { colorspace::RGB      , 12, packing::MethodA, endianness::BE },
    { colorspace::RGB      , 16, packing::Packed , endianness::LE },
    { colorspace::RGB      , 16, packing::Packed , endianness::BE },
    { colorspace::RGBA     ,  8, packing::Packed , endianness::LE },
    { colorspace::RGBA     , 10, packing::MethodA, endianness::LE },
    { colorspace::RGBA     , 10, packing::MethodA, endianness::BE },
    { colorspace::RGBA     , 12, packing::Packed , endianness::BE },
    { colorspace::RGBA     , 12, packing::MethodA, endianness::LE },
    { colorspace::RGBA     , 12, packing::MethodA, endianness::BE },
    { colorspace::RGBA     , 16, packing::Packed , endianness::LE },
    { colorspace::RGBA     , 16, packing::Packed , endianness::BE },
    { colorspace::Y        ,  8, packing::Packed , endianness::LE },
    { colorspace::Y        , 16, packing::Packed , endianness::LE },
    { colorspace::Y        , 16, packing::Packed , endianness::BE },
};
static_assert(dpx::flavor_Max == sizeof(DPX_Tested) / sizeof(dpx_tested), IncoherencyMessage);

struct dpx_also DPX_Also[] =
{
    { { colorspace::RGB      ,  8, packing::MethodA, endianness::LE}, dpx::flavor::Raw_RGB_8                 },
    { { colorspace::RGB      ,  8, packing::Packed , endianness::BE}, dpx::flavor::Raw_RGB_8                 },
    { { colorspace::RGB      ,  8, packing::MethodA, endianness::BE}, dpx::flavor::Raw_RGB_8                 },
    { { colorspace::RGB      , 16, packing::MethodA, endianness::LE}, dpx::flavor::Raw_RGB_16_LE             },
    { { colorspace::RGB      , 16, packing::MethodA, endianness::BE}, dpx::flavor::Raw_RGB_16_BE             },
    { { colorspace::RGBA     ,  8, packing::MethodA, endianness::LE}, dpx::flavor::Raw_RGBA_8                },
    { { colorspace::RGBA     ,  8, packing::Packed , endianness::BE}, dpx::flavor::Raw_RGBA_8                },
    { { colorspace::RGBA     ,  8, packing::MethodA, endianness::BE}, dpx::flavor::Raw_RGBA_8                },
    { { colorspace::RGBA     , 16, packing::MethodA, endianness::LE}, dpx::flavor::Raw_RGBA_16_LE            },
    { { colorspace::RGBA     , 16, packing::MethodA, endianness::BE}, dpx::flavor::Raw_RGBA_16_BE            },
    { { colorspace::Y        ,  8, packing::Packed , endianness::LE}, dpx::flavor::Raw_Y_8                   },
    { { colorspace::Y        ,  8, packing::MethodA, endianness::LE}, dpx::flavor::Raw_Y_8                   },
    { { colorspace::Y        ,  8, packing::Packed , endianness::BE}, dpx::flavor::Raw_Y_8                   },
    { { colorspace::Y        ,  8, packing::MethodA, endianness::BE}, dpx::flavor::Raw_Y_8                   },
    { { colorspace::Y        , 16, packing::MethodA, endianness::LE}, dpx::flavor::Raw_Y_16_LE               },
    { { colorspace::Y        , 16, packing::MethodA, endianness::BE}, dpx::flavor::Raw_Y_16_BE               },
};

//---------------------------------------------------------------------------
// Info
struct dpx_info
{
    uint8_t                     PixelsPerBlock;
    uint8_t                     BytesPerBlock;
};

struct dpx_info DPX_Info[] =
{
    { 1,  3 }, // 1x3x 8-bit in 1x24-bit
    { 1,  4 }, // 1x3x10-bit in 1x32-bit including 1x2-bit padding
    { 1,  4 }, // 1x3x10-bit in 1x32-bit including 1x2-bit padding
    { 8, 36 }, // 8x3x12-bit in 9x32-bit
    { 1,  6 }, // 1x3x12-bit in 3x16-bit including 3x4-bit padding
    { 1,  6 }, // 1x3x12-bit in 3x16-bit including 3x4-bit padding
    { 1,  6 }, // 1x3x16-bit in 3x16-bit
    { 1,  6 }, // 1x3x16-bit in 3x16-bit
    { 1,  4 }, // 1x4x 8-bit in 1x32-bit
    { 3, 16 }, // 3x4x10-bit in 3x40-bit,
    { 3, 16 }, // 3x4x10-bit in 3x40-bit,
    { 2, 12 }, // 2x4x12-bit in 2x48-bit, 
    { 1,  8 }, // 1x4x12-bit in 4x16-bit including 4x4-bit padding
    { 1,  8 }, // 1x4x12-bit in 4x16-bit including 4x4-bit padding
    { 1,  8 }, // 1x4x16-bit in 4x16-bit
    { 1,  8 }, // 1x4x16-bit in 4x16-bit
    { 1,  1 }, // 1x1x 8-bit in 1x 8-bit
    { 1,  2 }, // 1x1x16-bit in 1x16-bit
    { 1,  2 }, // 1x1x16-bit in 1x16-bit
};
static_assert(dpx::flavor_Max == sizeof(DPX_Info) / sizeof(dpx_info), IncoherencyMessage);

//***************************************************************************
// DPX
//***************************************************************************

//---------------------------------------------------------------------------
dpx::dpx(errors* Errors_Source) :
    input_base_uncompressed_video(Errors_Source, Parser_DPX, true)
{
}

//---------------------------------------------------------------------------
dpx::~dpx()
{
    delete[] HeaderCopy;
}

//---------------------------------------------------------------------------
void dpx::ParseBuffer()
{
    // Handle "same as the previous frame" content
    if (HeaderCopy)
    {
        // Size
        size_t HeaderCopy_Size = HeaderCopy_Info & 0xFFF;
        HeaderCopy_Size++;

        // Adapt previous frame content from new frame content
        uint32_t* HeaderCopy32 = (uint32_t*)HeaderCopy;
        const uint32_t* Buffer32 = (const uint32_t*)Buffer.Data();
        memmove(HeaderCopy + 36, Buffer.Data() + 36, 160 - 36); // Image filename + Creation date/time: yyyy:mm:dd:hh:mm:ssLTZ
        HeaderCopy32[1676 / 4] = Buffer32[1676 / 4]; // Count
        HeaderCopy32[1712 / 4] = Buffer32[1712 / 4]; // Frame position in sequence
        HeaderCopy32[1920 / 4] = Buffer32[1920 / 4]; // SMPTE time code
        HeaderCopy[1929] = Buffer[1929]; // Field number

        // Compare
        if (memcmp(HeaderCopy, Buffer.Data(), Buffer.Size() >= 2048 ? 2048 : Buffer.Size()))
            Invalid(invalid::DittoKey_NotSame);

        //TODO: no need to check again if the file is supported
    }

    // Test that it is a DPX
    if (Buffer.Size() < 4)
        return;

    dpx_tested Info;

    Buffer_Offset = 0;
    uint32_t MagicNumber = Get_B4();
    switch (MagicNumber)
    {
        case 0x58504453: // XPDS
            Info.Endianness = endianness::LE;
            IsBigEndian = false;
            break;
        case 0x53445058: // SDPX
            Info.Endianness = endianness::BE;
            IsBigEndian = true;
            break;
        default:
            return;
    }
    SetDetected();

    uint32_t OffsetToImageData = Get_X4();
    uint64_t VersionNumber = Get_B8() >> 24;
    switch (VersionNumber)
    {
    case 0x56312E3000LL:
    case 0x56322E3000LL:
    case 0x76312E3000LL:
        break;
    default:
        Undecodable(undecodable::VersionNumber);
        return;
    }

    Buffer_Offset = 28;
    uint32_t IndustryHeaderSize = Get_X4();
    if (IndustryHeaderSize == (uint32_t)-1)
        IndustryHeaderSize = 0;
    Buffer_Offset = 660;
    uint32_t Encryption = Get_X4();
    if (Encryption != (uint32_t)-1 && Encryption != 0) // One file found with Encryption of 0 but not encrypted, we accept it.
        Unsupported(unsupported::Encryption);
    Buffer_Offset = 768;
    if (Get_X2() != 0)
        Unsupported(unsupported::Orientation);
    if (Get_X2() != 1)
        Unsupported(unsupported::NumberOfElements);
    uint32_t Width = Get_X4();
    uint32_t Height = Get_X4();
    Buffer_Offset = 780;
    if (Get_X4() != 0)
        Unsupported(unsupported::DataSign);
    Buffer_Offset = 800;
    uint8_t Descriptor = Get_X1();
    switch (Descriptor)
    {
    case  6: Info.ColorSpace = colorspace::Y; break;
    case 50: Info.ColorSpace = colorspace::RGB; break;
    case 51: Info.ColorSpace = colorspace::RGBA; break;
    default: Info.ColorSpace = (decltype(Info.ColorSpace))-1;
    }
    Buffer_Offset = 803;
    Info.BitDepth = Get_X1();
    Info.Packing = (packing)Get_X2();
    uint16_t Encoding = Get_X2();
    if (Encoding)
        Unsupported(unsupported::Encoding);
    uint32_t OffsetToData = Get_X4();
    if (OffsetToData)
    {
        if (OffsetToData < 1664 || OffsetToData > Buffer.Size())
            Undecodable(undecodable::OffsetToData);
        if (OffsetToImageData != OffsetToData)
            Unsupported(unsupported::OffsetToImageData); // FFmpeg specific, it prioritizes OffsetToImageData over OffsetToData. TODO: remove this limitation when future internal encoder is used
    }
    else
        OffsetToData = OffsetToImageData;
    if (Get_X4() != 0)
        Unsupported(unsupported::EolPadding);
   
    if (IndustryHeaderSize && InputInfo)
    {
        Buffer_Offset = 1724;
        double FrameRate_Film = Get_XF4(); // Frame rate of original (frames/s) 
        Buffer_Offset = 1940;
        double FrameRate_Television = Get_XF4(); // Temporal sampling rate or frame rate (Hz)

        // Coherency of frame rate
        if (FrameRate_Film && FrameRate_Television && FrameRate_Film != FrameRate_Television)
            Unsupported(unsupported::FrameRate_Incoherent);

        // Availability of frame rate
        // We have lot of DPX files without frame rate info, using FFmpeg default (25 at the moment of writing)
        //if (!FrameRate_Film && !FrameRate_Television)
        //    Unsupported(unsupported::FrameRate_Unavailable);

        InputInfo->FrameRate = FrameRate_Film ? FrameRate_Film : FrameRate_Television;
    }

    // Supported?
    for (const auto& DPX_Tested_Item : DPX_Tested)
    {
        if (DPX_Tested_Item == Info)
        {
            Flavor = (decltype(Flavor))(&DPX_Tested_Item - DPX_Tested);
            break;
        }
    }
    if (Flavor == (decltype(Flavor))-1)
    {
        for (const auto& DPX_Also_Item : DPX_Also)
        {
            if (DPX_Also_Item.Test == Info)
            {
                Flavor = (decltype(Flavor))DPX_Also_Item.Flavor;
                break;
            }
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
    if (Info.BitDepth > 10)
        slice_x = slice_x * 3 / 2; // 1.5x more slices if 16-bit
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

    // Computing OffsetAfterData
    size_t ContentSize_Multiplier = BytesPerBlock((flavor)Flavor);
    size_t OffsetAfterData = OffsetToData + ContentSize_Multiplier * Width * Height / Slice_Multiplier;
    if (OffsetAfterData > Buffer.Size())
    {
        if (!Actions[Action_AcceptTruncated])
            Undecodable(undecodable::DataSize);
    }

    // Can we compress?
    if (!HasErrors())
        SetSupported();

    // Testing padding bits
    if (IsSupported() && MayHavePaddingBits() && !Actions[Action_AcceptTruncated] && Actions[Action_CheckPadding] && RAWcooked)
    {
        uint8_t Step = Info.BitDepth == 10 ? 4 : 2;
        uint8_t Mask = Info.BitDepth == 10 ? 0x3 : 0xF;
        size_t i = OffsetToData;
        if (IsBigEndian)
            i += Step - 1;
        for (; i < OffsetAfterData; i += Step)
            if (Buffer[i] & Mask)
                break;
        if (i < OffsetAfterData)
        {
            // Non-zero padding bit found, storing data
            auto Temp_Size = OffsetAfterData - OffsetToData;
            if (Temp_Size > In.Size())  // Reuse old buffer if any and big enough
                In.Create(Temp_Size);
            memset(In.Data(), 0x00, Temp_Size);
            for (; i < OffsetAfterData; i += Step)
                In[i - OffsetToData] = Buffer[i] & Mask;
        }
        else
            In.Clear();
    }
    else
        In.Clear();

    // Write RAWcooked file
    if (IsSupported() && RAWcooked)
    {
        RAWcooked->Unique = false;
        RAWcooked->BeforeData = Buffer.Data();
        RAWcooked->BeforeData_Size = OffsetToData;
        RAWcooked->AfterData = Buffer.Data() + OffsetAfterData;
        RAWcooked->AfterData_Size = Buffer.Size() - OffsetAfterData;
        RAWcooked->InData = In.Data();
        RAWcooked->InData_Size = In.Size();
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

    if (Actions[Action_Conch])
        ConformanceCheck();
}

//---------------------------------------------------------------------------
void dpx::BufferOverflow()
{
    Undecodable(undecodable::BufferOverflow);
}

//---------------------------------------------------------------------------
void dpx::ConformanceCheck()
{
    Buffer_Offset = 4;
    uint32_t OffsetToImageData = Get_X4();
    if (OffsetToImageData < 1664 || OffsetToImageData > Buffer.Size())
        Invalid(invalid::OffsetToImageData);
    uint64_t VersionNumber = Get_B8() >> 24;
    switch (VersionNumber)
    {
    case 0x76312E3000LL:
        Invalid(invalid::VersionNumber);
    default:;
    }
    Buffer_Offset = 16;
    uint32_t TotalImageFileSize = Get_X4();
    if (TotalImageFileSize != FileSize)
        Invalid(invalid::TotalImageFileSize);
    uint32_t DittoKey = Get_X4();
    if (DittoKey > 1 && DittoKey != (uint32_t)-1)
        Invalid(invalid::DittoKey);
    Buffer_Offset = 770;
    uint16_t NumberOfElements = Get_X2();
    if (NumberOfElements == 0 || NumberOfElements > 8)
    {
        Invalid(invalid::NumberOfElements);
        if (NumberOfElements)
            NumberOfElements = 8; // File has an issue, testing only the first 8 elements
    }
    if (Buffer_Offset + 72 * NumberOfElements > Buffer.Size())
        NumberOfElements = (uint16_t)((Buffer.Size() - Buffer_Offset) / 72); // File has an issue, testing element which can fit in file size
    Buffer_Offset = 804;
    bool HasEncoding = false;
    for (uint16_t i = 0; i < NumberOfElements; i++)
    {
        uint32_t Encoding = Get_X4();
        if (!HasEncoding && Encoding)
            HasEncoding = true;
        uint32_t OffsetToData = Get_X4();
        if (OffsetToData < 1664 || OffsetToData > Buffer.Size())
        {
            if (i) // if i == 0, already signaled in the common parsing
                Undecodable(undecodable::OffsetToData);
        }
        else if (OffsetToData < OffsetToImageData)
            Invalid(invalid::OffsetToImageData);
        Buffer_Offset += 68; // Next element
    }

    if (DittoKey == 0 && Buffer.Size() >= 1664)
    {
        // Copy header content so we compare content in next frames
        HeaderCopy_Info = OffsetToImageData;
        if (HeaderCopy_Info < 1664)
            HeaderCopy_Info = 1664; // Do not trust OffsetToImageData
        if (HeaderCopy_Info > 2048)
            HeaderCopy_Info = 2048; // Do not compare user data
        HeaderCopy = new uint8_t[2048];
        memmove(HeaderCopy, Buffer.Data(), HeaderCopy_Info >= 2048 ? 2048 : HeaderCopy_Info);
        HeaderCopy_Info--;
        HeaderCopy_Info |= (HasEncoding ? 1 : 0) << 12;
    }
}

//---------------------------------------------------------------------------
bool dpx::MayHavePaddingBits()
{
    if ((flavor)Flavor == (flavor)-1)
        return false;

    auto Flavor_BitDepth = DPX_Tested[(size_t)Flavor].BitDepth;
    auto Flavor_Packing = DPX_Tested[(size_t)Flavor].Packing;

    return (Flavor_BitDepth == 10 || Flavor_BitDepth == 12) && Flavor_Packing == packing::MethodA;
}

//---------------------------------------------------------------------------
string dpx::Flavor_String()
{
    return DPX_Flavor_String(Flavor);
}

//---------------------------------------------------------------------------
size_t dpx::BytesPerBlock(dpx::flavor Flavor)
{
    return DPX_Info[(size_t)Flavor].BytesPerBlock;
}

//---------------------------------------------------------------------------
size_t dpx::PixelsPerBlock(dpx::flavor Flavor)
{
    return DPX_Info[(size_t)Flavor].PixelsPerBlock;
}

//---------------------------------------------------------------------------
static colorspace ColorSpace(dpx::flavor Flavor)
{
    return DPX_Tested[(size_t)Flavor].ColorSpace;
}
static const char* ColorSpace_String(dpx::flavor Flavor)
{
    switch (ColorSpace(Flavor))
    {
    case colorspace::RGB : return "RGB";
    case colorspace::RGBA: return "RGBA";
    case colorspace::Y: return "Y";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
static uint8_t BitDepth(dpx::flavor Flavor)
{
    return DPX_Tested[(size_t)Flavor].BitDepth;
}
static const char* BitDepth_String(dpx::flavor Flavor)
{
    switch (DPX_Tested[(size_t)Flavor].BitDepth)
    {
    case 8 : return "8";
    case 10: return "10";
    case 12: return "12";
    case 16: return "16";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
static const char* Packing_String(dpx::flavor Flavor)
{
    switch (DPX_Tested[(size_t)Flavor].Packing)
    {
    case packing::Packed : return "Packed";
    case packing::MethodA: return "FilledA";
    case packing::MethodB: return "FilledB";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
static const char* Endianess_String(dpx::flavor Flavor)
{
    switch (DPX_Tested[(size_t)Flavor].Endianness)
    {
    case endianness::LE: return BitDepth(Flavor) == 8 ? "" : "LE";
    case endianness::BE: return "BE";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
string DPX_Flavor_String(uint8_t Flavor)
{
    string ToReturn("DPX/Raw/");
    ToReturn += ColorSpace_String((dpx::flavor)Flavor);
    ToReturn += '/';
    ToReturn += BitDepth_String((dpx::flavor)Flavor);
    ToReturn += "bit";
    const char* Value = Packing_String((dpx::flavor)Flavor);
    if (Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    Value = Endianess_String((dpx::flavor)Flavor);
    if (Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    return ToReturn;
}
