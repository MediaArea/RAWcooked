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
#include <cmath>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Tested cases
enum endianess
{
    LE,
    BE,
};
enum descriptor
{
    R           =   1,
    G           =   2,
    B           =   3,
    A           =   4,
    Y           =   6,
    ColorDiff   =   7,
    Z           =   8,
    Composite   =   9,
    RGB         =  50,
    RGBA        =  51,
    ABGR        =  52,
    CbYCrY      = 100,
    CbYACrYA    = 101,
    CbYCr       = 102,
    CbYCrA      = 103,
};
enum packing
{
    Packed,
    MethodA,
    MethodB,
};

struct dpx_tested
{
    descriptor                  Descriptor;
    uint8_t                     BitDepth;
    packing                     Packing;
    endianess                   Endianess;
    dpx::style                  Style;
};

const size_t DPX_Tested_Size = 26;
struct dpx_tested DPX_Tested[DPX_Tested_Size] =
{
    { RGB       ,  8, Packed , BE, dpx::RGB_8 },
    { RGB       ,  8, Packed , LE, dpx::RGB_8 },
    { RGB       ,  8, MethodA, BE, dpx::RGB_8 },
    { RGB       ,  8, MethodA, LE, dpx::RGB_8 },
    { RGB       , 10, MethodA, LE, dpx::RGB_10_FilledA_LE },
    { RGB       , 10, MethodA, BE, dpx::RGB_10_FilledA_BE },
    { RGB       , 12, Packed , BE, dpx::RGB_12_Packed_BE },
    { RGB       , 12, MethodA, BE, dpx::RGB_12_FilledA_BE },
    { RGB       , 12, MethodA, LE, dpx::RGB_12_FilledA_LE },
    { RGB       , 16, Packed , BE, dpx::RGB_16_BE },
    { RGB       , 16, Packed , LE, dpx::RGB_16_LE },
    { RGB       , 16, MethodA, BE, dpx::RGB_16_BE },
    { RGB       , 16, MethodA, LE, dpx::RGB_16_LE },
    { RGBA      ,  8, Packed , BE, dpx::RGBA_8 },
    { RGBA      ,  8, Packed , LE, dpx::RGBA_8 },
    { RGBA      ,  8, MethodA, BE, dpx::RGBA_8 },
    { RGBA      ,  8, MethodA, LE, dpx::RGBA_8 },
    { RGBA      , 10, MethodA, LE, dpx::RGBA_10_FilledA_LE },
    { RGBA      , 10, MethodA, BE, dpx::RGBA_10_FilledA_BE },
    { RGBA      , 12, Packed , BE, dpx::RGBA_12_Packed_BE },
    { RGBA      , 12, MethodA, BE, dpx::RGBA_12_FilledA_BE },
    { RGBA      , 12, MethodA, LE, dpx::RGBA_12_FilledA_LE },
    { RGBA      , 16, Packed , BE, dpx::RGBA_16_BE },
    { RGBA      , 16, Packed , LE, dpx::RGBA_16_LE },
    { RGBA      , 16, MethodA, BE, dpx::RGBA_16_BE },
    { RGBA      , 16, MethodA, LE, dpx::RGBA_16_LE },
};

//---------------------------------------------------------------------------

//***************************************************************************
// Errors
//***************************************************************************

//---------------------------------------------------------------------------
const char* dpx::ErrorMessage()
{
    return error_message;
}

//***************************************************************************
// DPX
//***************************************************************************

//---------------------------------------------------------------------------
dpx::dpx() :
    RAWcooked(NULL),
    IsDetected(false),
    Style(DPX_Style_Max),
    FrameRate(NULL),
    error_message(NULL)
{
}

//---------------------------------------------------------------------------
uint16_t dpx::Get_L2()
{
    uint16_t ToReturn = Buffer[Buffer_Offset + 0] | (Buffer[Buffer_Offset + 1] << 8);
    Buffer_Offset += 2;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint16_t dpx::Get_B2()
{
    uint16_t ToReturn = (Buffer[Buffer_Offset + 0] << 8) | Buffer[Buffer_Offset + 1];
    Buffer_Offset += 2;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint32_t dpx::Get_L4()
{
    uint32_t ToReturn = Buffer[Buffer_Offset+0] | (Buffer[Buffer_Offset + 1] << 8) | (Buffer[Buffer_Offset + 2] << 16) | (Buffer[Buffer_Offset + 3] << 24);
    Buffer_Offset += 4;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint32_t dpx::Get_B4()
{
    uint32_t ToReturn = (Buffer[Buffer_Offset + 0] << 24) | (Buffer[Buffer_Offset + 1] << 16) | (Buffer[Buffer_Offset + 2] << 8) | Buffer[Buffer_Offset + 3];
    Buffer_Offset += 4;
    return ToReturn;
}

//---------------------------------------------------------------------------
double dpx::Get_XF4()
{
    // sign          1 bit
    // exponent      8 bit
    // significand  23 bit

    // Retrieving data
    uint32_t Integer = Get_X4();

    // Retrieving elements
    bool     Sign     = (Integer >> 31) ? true : false;
    uint32_t Exponent = (Integer >> 23) & 0xFF;
    uint32_t Mantissa =  Integer        & 0x007FFFFF;

    // Some computing
    if (Exponent == 0 || Exponent == 0xFF)
        return 0; // These are denormalised numbers, NANs, and other horrible things
    Exponent -= 0x7F; // Bias
    double Answer = (((double)Mantissa) / 8388608 + 1.0)*std::pow((double)2, (int)Exponent); // (1+Mantissa) * 2^Exponent
    if (Sign)
        Answer = -Answer;

    return Answer;
}

//---------------------------------------------------------------------------
bool dpx::Parse()
{
    if (Buffer_Size < 1664)
        return Error(NULL);

    Buffer_Offset = 0;
    uint32_t Magic = Get_B4();
    switch (Magic)
    {
        case 0x58504453: // XPDS
            IsBigEndian = false;
            break;
        case 0x53445058: // SDPX
            IsBigEndian = true;
            break;
        default:
            return Error(NULL);
    }
    IsDetected = true;
    uint32_t OffsetToImage = Get_X4();
    if (OffsetToImage > Buffer_Size)
        return Error("Offset to image data in bytes");
    uint32_t VersioNumber = Get_B4();
    switch (VersioNumber)
    {
        case 0x56312E30: // V1.0
        case 0x56322E30: // V2.0
                        break;
        default:        return Error("Version number of header format");
    }
    Buffer_Offset = 28;
    uint32_t IndustryHeaderSize = Get_X4();
    if (IndustryHeaderSize == (uint32_t)-1)
        IndustryHeaderSize = 0;
    Buffer_Offset = 660;
    uint32_t Encryption = Get_X4();
    if (Encryption != 0xFFFFFFFF && Encryption != 0) // One file found with Encryption of 0 but not encrypted, we accept it.
        return Error("Encryption key");
    Buffer_Offset = 768;
    if (Get_X2() != 0)
        return Error("Image orientation");
    if (Get_X2() != 1)
        return Error("Number of image elements");
    uint32_t Width = Get_X4();
    uint32_t Height = Get_X4();
    Buffer_Offset = 780;
    if (Get_X4() != 0)
        return Error("Data sign");
    Buffer_Offset = 800;
    uint8_t Descriptor = Get_X1();
    Buffer_Offset = 803;
    uint8_t BitDepth = Get_X1();
    uint16_t Packing = Get_X2();
    if (Get_X2() != 0)
        return Error("Encoding");
    uint32_t OffsetToData = Get_X4();
    if (OffsetToData != OffsetToImage)
        return Error("Offset to data");
    if (Get_X4() != 0)
        return Error("End-of-line padding");
    //uint32_t EndOfImagePadding = Get_X4(); //We do not rely on EndOfImagePadding and compute the end of content based on other fields
    
    if (IndustryHeaderSize && FrameRate)
    {
        Buffer_Offset = 1724;
        double FrameRate_Film = Get_XF4(); // Frame rate of original (frames/s) 
        Buffer_Offset = 1940;
        double FrameRate_Television = Get_XF4(); // Temporal sampling rate or frame rate (Hz)

        // Integrity of frame rate
        if (FrameRate_Film && FrameRate_Television && FrameRate_Film != FrameRate_Television)
            return Error("\"Frame rate of original (frames/s)\" not same as \"Temporal sampling rate or frame rate (Hz)\"");

        // Availability of frame rate
        // We have lot of DPX files without frame rate info, using FFmpeg default (25 at the moment of writing)
        //if (!FrameRate_Film && !FrameRate_Television)
        //    return Error("Frame rate not available in DPX header");

        double FrameRateD = FrameRate_Film ? FrameRate_Film : FrameRate_Television;
        if (FrameRateD)
        {
            stringstream ss;
            ss.precision(11);
            ss << FrameRateD;
            *FrameRate = ss.str();
        }
    }

    // Supported?
    size_t Tested = 0;
    for (; Tested < DPX_Tested_Size; Tested++)
    {
        dpx_tested& DPX_Tested_Item = DPX_Tested[Tested];
        if (DPX_Tested_Item.Descriptor == Descriptor
            && DPX_Tested_Item.BitDepth == BitDepth
            && DPX_Tested_Item.Packing == Packing
            && DPX_Tested_Item.Endianess == (IsBigEndian ? BE : LE))
            break;
    }
    if (Tested >= DPX_Tested_Size)
        return Error("Style (Descriptor / BitDepth / Packing / Endianess combination)");
    Style = DPX_Tested[Tested].Style;

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
    size_t Slice_Multiplier = PixelsPerBlock((style)Style);
    if (Slice_Multiplier == 0)
        return Error("(Internal error)");
    for (; slice_x; slice_x--)
    {
        if (Width % (slice_x * Slice_Multiplier) == 0)
            break;
    }
    if (slice_x == 0)
        return Error("Pixels in slice not on a 32-bit boundary");

    // Computing EndOfImagePadding
    size_t ContentSize_Multiplier = BitsPerBlock((style)Style);
    if (ContentSize_Multiplier == 0)
        return Error("(Internal error)");
    size_t EndOfImagePadding = Buffer_Size - (OffsetToImage + ContentSize_Multiplier * Width * Height / Slice_Multiplier / 8);

    // Write RAWcooked file
    if (RAWcooked)
    {
        RAWcooked->Unique = false;
        RAWcooked->Before = Buffer;
        RAWcooked->Before_Size = OffsetToImage;
        RAWcooked->After = Buffer + Buffer_Size - EndOfImagePadding;
        RAWcooked->After_Size = EndOfImagePadding;
        RAWcooked->Parse();
    }

    return 0;
}

//---------------------------------------------------------------------------
size_t dpx::BitsPerBlock(dpx::style Style)
{
    switch (Style)
    {
        case dpx::RGB_8:                // 3x8-bit content
                                        return 24;
        case dpx::RGB_10_FilledA_BE:    // 3x10-bit content + 2-bit zero-padding in 32-bit
        case dpx::RGB_10_FilledA_LE:    // 3x10-bit content + 2-bit zero-padding in 32-bit
        case dpx::RGBA_8:               // 4x8-bit content
                                        return 32;
        case dpx::RGB_12_FilledA_BE:    // 3x(12-bit content + 4-bit zero-padding)
        case dpx::RGB_12_FilledA_LE:    // 3x(12-bit content + 4-bit zero-padding)
        case dpx::RGB_16_BE:            // 3x16-bit content
        case dpx::RGB_16_LE:            // 3x16-bit content
                                        return 48; 
        case dpx::RGBA_12_FilledA_BE:   // 4x(12-bit content + 4-bit zero-padding)
        case dpx::RGBA_12_FilledA_LE:   // 4x(12-bit content + 4-bit zero-padding)
        case dpx::RGBA_16_BE:           // 4x16-bit content
        case dpx::RGBA_16_LE:           // 4x16-bit content
                                        return 64; 
        case dpx::RGBA_12_Packed_BE:    // 4x12-bit content, multiplied by 2 pixels in a supported block
                                        return 96; 
        case dpx::RGBA_10_FilledA_BE:   // 4x10-bit content, 2-bit padding every 30-bit, multiplied by 3 pixels in a supported block
        case dpx::RGBA_10_FilledA_LE:   // 4x10-bit content, 2-bit padding every 30-bit, multiplied by 3 pixels in a supported block
                                        return 128; 
        case dpx::RGB_12_Packed_BE:     // 3x12-bit content, multiplied by 8 pixels in a supported block
                                        return 288;
        default:
                                        return 0;
    }
}

//---------------------------------------------------------------------------
size_t dpx::PixelsPerBlock(dpx::style Style)
{
    switch (Style)
    {
        case dpx::RGB_8:
        case dpx::RGBA_8:
        case dpx::RGB_10_FilledA_BE:
        case dpx::RGB_10_FilledA_LE:
        case dpx::RGB_12_FilledA_BE:
        case dpx::RGB_12_FilledA_LE:
        case dpx::RGB_16_BE:
        case dpx::RGB_16_LE:
        case dpx::RGBA_12_FilledA_BE:
        case dpx::RGBA_12_FilledA_LE:
        case dpx::RGBA_16_BE:
        case dpx::RGBA_16_LE:
                                        return 1;
        case dpx::RGBA_12_Packed_BE:    // 2x4x12-bit in 3x32-bit
                                        return 2;
        case dpx::RGBA_10_FilledA_BE:   // 3x4x10-bit in 3x32-bit
        case dpx::RGBA_10_FilledA_LE:   // 3x4x10-bit in 3x32-bit
                                        return 3;
        case dpx::RGB_12_Packed_BE:     // 8x3x12-bit in 9x32-bit
                                        return 8;
        default:
                                        return 0;
    }
}
