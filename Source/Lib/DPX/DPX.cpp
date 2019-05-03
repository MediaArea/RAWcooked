/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/DPX/DPX.h"
#include "Lib/RAWcooked/RAWcooked.h"
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
    "Flavor (Descriptor / BitDepth / Packing / Endianess combination)",
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
    "Ditto key",
    "Ditto key is set to \"same as the previous frame\" but header data differs",
    "Number of image elements",
};

enum code : uint8_t
{
    OffsetToImageData,
    TotalImageFileSize,
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

static_assert(error::Type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // dpx_issue

using namespace dpx_issue;

//---------------------------------------------------------------------------
// Tested cases
struct dpx_tested
{
    dpx::encoding               Encoding;
    dpx::descriptor             Descriptor;
    uint8_t                     BitDepth;
    dpx::packing                Packing;
    dpx::endianess              Endianess;
    dpx::flavor                 Flavor;
};

struct dpx_tested DPX_Tested[] =
{
    { dpx::Raw  , dpx::RGB      ,  8, dpx::Packed , dpx::BE, dpx::Raw_RGB_8                 },
    { dpx::Raw  , dpx::RGB      ,  8, dpx::Packed , dpx::LE, dpx::Raw_RGB_8                 },
    { dpx::Raw  , dpx::RGB      ,  8, dpx::MethodA, dpx::BE, dpx::Raw_RGB_8                 },
    { dpx::Raw  , dpx::RGB      ,  8, dpx::MethodA, dpx::LE, dpx::Raw_RGB_8                 },
    { dpx::Raw  , dpx::RGB      , 10, dpx::MethodA, dpx::LE, dpx::Raw_RGB_10_FilledA_LE     },
    { dpx::Raw  , dpx::RGB      , 10, dpx::MethodA, dpx::BE, dpx::Raw_RGB_10_FilledA_BE     },
    { dpx::Raw  , dpx::RGB      , 12, dpx::Packed , dpx::BE, dpx::Raw_RGB_12_Packed_BE      },
    { dpx::Raw  , dpx::RGB      , 12, dpx::MethodA, dpx::BE, dpx::Raw_RGB_12_FilledA_BE     },
    { dpx::Raw  , dpx::RGB      , 12, dpx::MethodA, dpx::LE, dpx::Raw_RGB_12_FilledA_LE     },
    { dpx::Raw  , dpx::RGB      , 16, dpx::Packed , dpx::BE, dpx::Raw_RGB_16_BE             },
    { dpx::Raw  , dpx::RGB      , 16, dpx::Packed , dpx::LE, dpx::Raw_RGB_16_LE             },
    { dpx::Raw  , dpx::RGB      , 16, dpx::MethodA, dpx::BE, dpx::Raw_RGB_16_BE             },
    { dpx::Raw  , dpx::RGB      , 16, dpx::MethodA, dpx::LE, dpx::Raw_RGB_16_LE             },
    { dpx::Raw  , dpx::RGBA     ,  8, dpx::Packed , dpx::BE, dpx::Raw_RGBA_8                },
    { dpx::Raw  , dpx::RGBA     ,  8, dpx::Packed , dpx::LE, dpx::Raw_RGBA_8                },
    { dpx::Raw  , dpx::RGBA     ,  8, dpx::MethodA, dpx::BE, dpx::Raw_RGBA_8                },
    { dpx::Raw  , dpx::RGBA     ,  8, dpx::MethodA, dpx::LE, dpx::Raw_RGBA_8                },
    { dpx::Raw  , dpx::RGBA     , 10, dpx::MethodA, dpx::LE, dpx::Raw_RGBA_10_FilledA_LE    },
    { dpx::Raw  , dpx::RGBA     , 10, dpx::MethodA, dpx::BE, dpx::Raw_RGBA_10_FilledA_BE    },
    { dpx::Raw  , dpx::RGBA     , 12, dpx::Packed , dpx::BE, dpx::Raw_RGBA_12_Packed_BE     },
    { dpx::Raw  , dpx::RGBA     , 12, dpx::MethodA, dpx::BE, dpx::Raw_RGBA_12_FilledA_BE    },
    { dpx::Raw  , dpx::RGBA     , 12, dpx::MethodA, dpx::LE, dpx::Raw_RGBA_12_FilledA_LE    },
    { dpx::Raw  , dpx::RGBA     , 16, dpx::Packed , dpx::BE, dpx::Raw_RGBA_16_BE            },
    { dpx::Raw  , dpx::RGBA     , 16, dpx::Packed , dpx::LE, dpx::Raw_RGBA_16_LE            },
    { dpx::Raw  , dpx::RGBA     , 16, dpx::MethodA, dpx::BE, dpx::Raw_RGBA_16_BE            },
    { dpx::Raw  , dpx::RGBA     , 16, dpx::MethodA, dpx::LE, dpx::Raw_RGBA_16_LE            },
};
const size_t DPX_Tested_Size = sizeof(DPX_Tested) / sizeof(dpx_tested);

//---------------------------------------------------------------------------

//***************************************************************************
// DPX
//***************************************************************************

//---------------------------------------------------------------------------
dpx::dpx(errors* Errors_Source) :
    input_base_uncompressed(Errors_Source, Parser_DPX, true)
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
        uint32_t* Buffer32 = (uint32_t*)Buffer;
        memmove(HeaderCopy + 36, Buffer + 36, 160 - 36); // Image filename + Creation date/time: yyyy:mm:dd:hh:mm:ssLTZ
        HeaderCopy32[1676 / 4] = Buffer32[1676 / 4]; // Count
        HeaderCopy32[1712 / 4] = Buffer32[1712 / 4]; // Frame position in sequence
        HeaderCopy32[1920 / 4] = Buffer32[1920 / 4]; // SMPTE time code
        HeaderCopy[1929] = Buffer[1929]; // Field number

        // Compare
        if (memcmp(HeaderCopy, Buffer, Buffer_Size >= 2048 ? 2048 : Buffer_Size))
            Invalid(invalid::DittoKey_NotSame);

        //TODO: no need to check again if the file is supported
    }

    // Test that it is a DPX
    if (Buffer_Size < 4)
        return;
    Buffer_Offset = 0;
    uint32_t MagicNumber = Get_B4();
    switch (MagicNumber)
    {
        case 0x58504453: // XPDS
            IsBigEndian = false;
            break;
        case 0x53445058: // SDPX
            IsBigEndian = true;
            break;
        default:
            return;
    }
    SetDetected();

    uint32_t OffsetToImageData = Get_X4();
    uint64_t VersionNumber = Get_B8() >> 24;
    if (VersionNumber != 0x56312E3000LL && VersionNumber != 0x56322E3000LL)
    {
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
    Buffer_Offset = 803;
    uint8_t BitDepth = Get_X1();
    uint16_t Packing = Get_X2();
    uint16_t Encoding = Get_X2();
    uint32_t OffsetToData = Get_X4();
    if (OffsetToData < 1664 || OffsetToData > Buffer_Size)
        Undecodable(undecodable::OffsetToData);
    if (OffsetToImageData != OffsetToData)
        Unsupported(unsupported::OffsetToImageData); // FFmpeg specific, it prioritizes OffsetToImageData over OffsetToData. TODO: remove this limitation when future internal encoder is used
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
    size_t Tested = 0;
    for (; Tested < DPX_Tested_Size; Tested++)
    {
        dpx_tested& DPX_Tested_Item = DPX_Tested[Tested];
        if (DPX_Tested_Item.Descriptor == Descriptor
            && DPX_Tested_Item.Encoding == Encoding
            && DPX_Tested_Item.BitDepth == BitDepth
            && DPX_Tested_Item.Packing == Packing
            && DPX_Tested_Item.Endianess == (IsBigEndian ? BE : LE))
            break;
    }
    if (Tested >= DPX_Tested_Size)
    {
        Unsupported(unsupported::Flavor);
        return;
    }
    Flavor = DPX_Tested[Tested].Flavor;

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
    slice_y = slice_x;
    if (BitDepth > 10)
        slice_x = slice_x * 3 / 2; // 1.5x more slices if 16-bit

    // Computing which slice count is suitable // TODO: smarter algo, currently only computing in order to have pixels not accross 2 32-bit words
    size_t Slice_Multiplier = PixelsPerBlock((flavor)Flavor);
    if (Slice_Multiplier == 0)
    {
        Unsupported(unsupported::InternalError);
        return;
    }
    for (; slice_x; slice_x--)
    {
        if (Width % (slice_x * Slice_Multiplier) == 0)
            break;
    }
    if (slice_x == 0)
        Unsupported(unsupported::PixelBoundaries);

    // Computing OffsetAfterData
    size_t ContentSize_Multiplier = BitsPerBlock((flavor)Flavor);
    if (ContentSize_Multiplier == 0)
    {
        Unsupported(unsupported::InternalError);
        return;
    }
    size_t OffsetAfterData = OffsetToData + ContentSize_Multiplier * Width * Height / Slice_Multiplier / 8;
    if (OffsetAfterData > Buffer_Size)
    {
        if (!Actions[Action_AcceptTruncated])
            Undecodable(undecodable::DataSize);
    }

    // Can we compress?
    if (!HasErrors())
        SetSupported();

    // Testing padding bits
    uint8_t* In = nullptr;
    size_t In_Size = 0;
    if (IsSupported() && !Actions[Action_AcceptTruncated] && Actions[Action_CheckPadding])
    {
        if (Encoding == Raw && (BitDepth == 10 || BitDepth == 12) && Packing == MethodA)
        {
            size_t Step = BitDepth == 10 ? 4 : 2;
            bool IsNOK = false;
            for (size_t i = OffsetToData + (IsBigEndian ? (Step - 1) : 0); i < OffsetAfterData; i += Step)
                if (Buffer[i] & 0x3)
                    IsNOK = true;
            if (IsNOK)
            {
                In_Size = OffsetAfterData - OffsetToData;
                In = new uint8_t[In_Size];
                memset(In, 0x00, In_Size);
                for (size_t i = (IsBigEndian ? (Step - 1) : 0); i < In_Size; i += Step)
                    In[i] = Buffer[OffsetToData + i] & 0x3;
            }
        }
    }

    // Write RAWcooked file
    if (IsSupported() && RAWcooked)
    {
        RAWcooked->Unique = false;
        RAWcooked->BeforeData = Buffer;
        RAWcooked->BeforeData_Size = OffsetToData;
        RAWcooked->AfterData = Buffer + OffsetAfterData;
        RAWcooked->AfterData_Size = Buffer_Size - OffsetAfterData;
        RAWcooked->InData = In;
        RAWcooked->InData_Size = In_Size;
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
string dpx::Flavor_String()
{
    return DPX_Flavor_String(Flavor);
}

//---------------------------------------------------------------------------
size_t dpx::BitsPerBlock(dpx::flavor Flavor)
{
    switch (Flavor)
    {
        case dpx::Raw_RGB_8:                // 3x8-bit content
                                        return 24;
        case dpx::Raw_RGB_10_FilledA_BE:    // 3x10-bit content + 2-bit zero-padding in 32-bit
        case dpx::Raw_RGB_10_FilledA_LE:    // 3x10-bit content + 2-bit zero-padding in 32-bit
        case dpx::Raw_RGBA_8:               // 4x8-bit content
                                        return 32;
        case dpx::Raw_RGB_12_FilledA_BE:    // 3x(12-bit content + 4-bit zero-padding)
        case dpx::Raw_RGB_12_FilledA_LE:    // 3x(12-bit content + 4-bit zero-padding)
        case dpx::Raw_RGB_16_BE:            // 3x16-bit content
        case dpx::Raw_RGB_16_LE:            // 3x16-bit content
                                        return 48; 
        case dpx::Raw_RGBA_12_FilledA_BE:   // 4x(12-bit content + 4-bit zero-padding)
        case dpx::Raw_RGBA_12_FilledA_LE:   // 4x(12-bit content + 4-bit zero-padding)
        case dpx::Raw_RGBA_16_BE:           // 4x16-bit content
        case dpx::Raw_RGBA_16_LE:           // 4x16-bit content
                                        return 64; 
        case dpx::Raw_RGBA_12_Packed_BE:    // 4x12-bit content, multiplied by 2 pixels in a supported block
                                        return 96; 
        case dpx::Raw_RGBA_10_FilledA_BE:   // 4x10-bit content, 2-bit padding every 30-bit, multiplied by 3 pixels in a supported block
        case dpx::Raw_RGBA_10_FilledA_LE:   // 4x10-bit content, 2-bit padding every 30-bit, multiplied by 3 pixels in a supported block
                                        return 128; 
        case dpx::Raw_RGB_12_Packed_BE:     // 3x12-bit content, multiplied by 8 pixels in a supported block
                                        return 288;
        default:
                                        return 0;
    }
}

//---------------------------------------------------------------------------
size_t dpx::PixelsPerBlock(dpx::flavor Flavor)
{
    switch (Flavor)
    {
        case dpx::Raw_RGB_8:
        case dpx::Raw_RGBA_8:
        case dpx::Raw_RGB_10_FilledA_BE:
        case dpx::Raw_RGB_10_FilledA_LE:
        case dpx::Raw_RGB_12_FilledA_BE:
        case dpx::Raw_RGB_12_FilledA_LE:
        case dpx::Raw_RGB_16_BE:
        case dpx::Raw_RGB_16_LE:
        case dpx::Raw_RGBA_12_FilledA_BE:
        case dpx::Raw_RGBA_12_FilledA_LE:
        case dpx::Raw_RGBA_16_BE:
        case dpx::Raw_RGBA_16_LE:
                                        return 1;
        case dpx::Raw_RGBA_12_Packed_BE:    // 2x4x12-bit in 3x32-bit
                                        return 2;
        case dpx::Raw_RGBA_10_FilledA_BE:   // 3x4x10-bit in 3x32-bit
        case dpx::Raw_RGBA_10_FilledA_LE:   // 3x4x10-bit in 3x32-bit
                                        return 3;
        case dpx::Raw_RGB_12_Packed_BE:     // 8x3x12-bit in 9x32-bit
                                        return 8;
        default:
                                        return 0;
    }
}

//---------------------------------------------------------------------------
dpx::descriptor dpx::ColorSpace(dpx::flavor Flavor)
{
    switch (Flavor)
    {
        case Raw_RGB_8:
        case Raw_RGB_10_FilledA_BE:
        case Raw_RGB_10_FilledA_LE:
        case Raw_RGB_12_FilledA_BE:
        case Raw_RGB_12_FilledA_LE:
        case Raw_RGB_12_Packed_BE:
        case Raw_RGB_16_BE:
        case Raw_RGB_16_LE:
                                        return RGB;
        case Raw_RGBA_8:
        case Raw_RGBA_10_FilledA_BE:
        case Raw_RGBA_10_FilledA_LE:
        case Raw_RGBA_12_FilledA_BE:
        case Raw_RGBA_12_FilledA_LE:
        case Raw_RGBA_16_BE:
        case Raw_RGBA_16_LE:
        case Raw_RGBA_12_Packed_BE:
                                        return RGBA;
        default:
                                        return (dpx::descriptor)-1;
    }
}
const char* dpx::ColorSpace_String(dpx::flavor Flavor)
{
    dpx::descriptor Value = dpx::ColorSpace(Flavor);

    switch (Value)
    {
        case RGB:
                                        return "RGB";
        case RGBA:
                                        return "RGBA";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
uint8_t dpx::BitDepth(dpx::flavor Flavor)
{
    switch (Flavor)
    {
        case Raw_RGB_8:
        case Raw_RGBA_8:
                                        return 8;
        case Raw_RGB_10_FilledA_BE:
        case Raw_RGB_10_FilledA_LE:
        case Raw_RGBA_10_FilledA_BE:
        case Raw_RGBA_10_FilledA_LE:
                                        return 10;
        case Raw_RGB_12_FilledA_BE:
        case Raw_RGB_12_FilledA_LE:
        case Raw_RGB_12_Packed_BE:
        case Raw_RGBA_12_FilledA_BE:
        case Raw_RGBA_12_FilledA_LE:
        case Raw_RGBA_12_Packed_BE:
                                        return 12;
        case Raw_RGB_16_BE:
        case Raw_RGB_16_LE:
        case Raw_RGBA_16_BE:
        case Raw_RGBA_16_LE:
                                        return 16;
        default:
                                        return 0;
    }
}
const char* dpx::BitDepth_String(dpx::flavor Flavor)
{
    uint8_t Value = BitDepth(Flavor);

    switch (Value)
    {
        case 8:
                                        return "8";
        case 10:
                                        return "10";
        case 12:
                                        return "12";
        case 16:
                                        return "16";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
dpx::packing dpx::Packing(dpx::flavor Flavor)
{
    switch (Flavor)
    {
        case Raw_RGB_10_FilledA_BE:
        case Raw_RGB_10_FilledA_LE:
        case Raw_RGBA_10_FilledA_BE:
        case Raw_RGBA_10_FilledA_LE:
        case Raw_RGB_12_FilledA_BE:
        case Raw_RGB_12_FilledA_LE:
        case Raw_RGBA_12_FilledA_BE:
        case Raw_RGBA_12_FilledA_LE:
                                        return MethodA;
        case Raw_RGB_12_Packed_BE:
        case Raw_RGBA_12_Packed_BE:
                                        return Packed;
        default:
                                        return (packing)-1;
    }
}
const char* dpx::Packing_String(dpx::flavor Flavor)
{
    dpx::packing Value = Packing(Flavor);

    switch (Value)
    {
        case Packed:
                                        return "Packed";
        case MethodA:
                                        return "FilledA";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
dpx::endianess dpx::Endianess(dpx::flavor Flavor)
{
    switch (Flavor)
    {
        case Raw_RGB_10_FilledA_LE:
        case Raw_RGB_12_FilledA_LE:
        case Raw_RGB_16_LE:
        case Raw_RGBA_10_FilledA_LE:
        case Raw_RGBA_12_FilledA_LE:
        case Raw_RGBA_16_LE:
                                        return LE;
        case Raw_RGB_10_FilledA_BE:
        case Raw_RGB_12_Packed_BE:
        case Raw_RGB_12_FilledA_BE:
        case Raw_RGB_16_BE:
        case Raw_RGBA_10_FilledA_BE:
        case Raw_RGBA_12_Packed_BE:
        case Raw_RGBA_12_FilledA_BE:
        case Raw_RGBA_16_BE:
                                        return BE;
        default:
                                        return (dpx::endianess)-1;
    }
}
const char* dpx::Endianess_String(dpx::flavor Flavor)
{
    dpx::endianess Value = Endianess(Flavor);

    switch (Value)
    {
        case LE:
                                        return "LE";
        case BE:
                                        return "BE";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
string DPX_Flavor_String(uint8_t Flavor)
{
    string ToReturn("DPX/Raw/");
    ToReturn += dpx::ColorSpace_String((dpx::flavor)Flavor);
    ToReturn += '/';
    ToReturn += dpx::BitDepth_String((dpx::flavor)Flavor);
    ToReturn += "bit";
    const char* Value = dpx::Packing_String((dpx::flavor)Flavor);
    if (Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    Value = dpx::Endianess_String((dpx::flavor)Flavor);
    if (Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    return ToReturn;
}

//---------------------------------------------------------------------------
void dpx::ConformanceCheck()
{
    Buffer_Offset = 4;
    uint32_t OffsetToImageData = Get_X4();
    if (OffsetToImageData < 1664 || OffsetToImageData > Buffer_Size)
        Invalid(invalid::OffsetToImageData);
    Buffer_Offset = 16;
    uint32_t TotalImageFileSize = Get_X4();
    if (TotalImageFileSize != Buffer_Size)
        Invalid(invalid::TotalImageFileSize);
    uint32_t DittoKey = Get_X4();
    if (DittoKey > 1)
        Invalid(invalid::DittoKey);
    Buffer_Offset = 770;
    uint16_t NumberOfElements = Get_X2();
    if (NumberOfElements == 0 || NumberOfElements > 8)
    {
        Invalid(invalid::NumberOfElements);
        if (NumberOfElements)
            NumberOfElements = 8; // File has an issue, testing only the first 8 elements
    }
    if (Buffer_Offset + 72 * NumberOfElements > Buffer_Size)
        NumberOfElements = (uint16_t) ((Buffer_Size - Buffer_Offset) / 72); // File has an issue, testing element which can fit in file size
    Buffer_Offset = 804;
    bool HasEncoding = false;
    for (uint16_t i = 0; i < NumberOfElements; i++)
    {
        uint32_t Encoding = Get_X4();
        if (!HasEncoding && Encoding)
            HasEncoding = true;
        uint32_t OffsetToData = Get_X4();
        if (OffsetToData < 1664 || OffsetToData > Buffer_Size)
        {
            if (i) // if i == 0, already signaled in the common parsing
                Undecodable(undecodable::OffsetToData);
        }
        else if (OffsetToData < OffsetToImageData)
            Invalid(invalid::OffsetToImageData);
        Buffer_Offset += 68; // Next element
    }

    if (DittoKey == 0 && Buffer_Size >= 1664)
    {
        // Copy header content so we compare content in next frames
        HeaderCopy_Info = OffsetToImageData;
        if (HeaderCopy_Info < 1664)
            HeaderCopy_Info = 1664; // Do not trust OffsetToImageData
        if (HeaderCopy_Info > 2048)
            HeaderCopy_Info = 2048; // Do not compare user data
        HeaderCopy = new uint8_t[2048];
        memmove(HeaderCopy, Buffer, HeaderCopy_Info >= 2048 ? 2048 : HeaderCopy_Info);
        HeaderCopy_Info--;
        HeaderCopy_Info |= (HasEncoding ? 1 : 0) << 12;
    }
}
