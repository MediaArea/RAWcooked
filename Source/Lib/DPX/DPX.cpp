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
struct dpx_tested
{
    dpx::encoding               Encoding;
    dpx::descriptor             Descriptor;
    uint8_t                     BitDepth;
    dpx::packing                Packing;
    dpx::endianess              Endianess;
    dpx::style                  Style;
};

const size_t DPX_Tested_Size = 26;
struct dpx_tested DPX_Tested[DPX_Tested_Size] =
{
    { dpx::Raw  , dpx::RGB      ,  8, dpx::Packed , dpx::BE, dpx::Raw_RGB_8 },
    { dpx::Raw  , dpx::RGB      ,  8, dpx::Packed , dpx::LE, dpx::Raw_RGB_8 },
    { dpx::Raw  , dpx::RGB      ,  8, dpx::MethodA, dpx::BE, dpx::Raw_RGB_8 },
    { dpx::Raw  , dpx::RGB      ,  8, dpx::MethodA, dpx::LE, dpx::Raw_RGB_8 },
    { dpx::Raw  , dpx::RGB      , 10, dpx::MethodA, dpx::LE, dpx::Raw_RGB_10_FilledA_LE },
    { dpx::Raw  , dpx::RGB      , 10, dpx::MethodA, dpx::BE, dpx::Raw_RGB_10_FilledA_BE },
    { dpx::Raw  , dpx::RGB      , 12, dpx::Packed , dpx::BE, dpx::Raw_RGB_12_Packed_BE },
    { dpx::Raw  , dpx::RGB      , 12, dpx::MethodA, dpx::BE, dpx::Raw_RGB_12_FilledA_BE },
    { dpx::Raw  , dpx::RGB      , 12, dpx::MethodA, dpx::LE, dpx::Raw_RGB_12_FilledA_LE },
    { dpx::Raw  , dpx::RGB      , 16, dpx::Packed , dpx::BE, dpx::Raw_RGB_16_BE },
    { dpx::Raw  , dpx::RGB      , 16, dpx::Packed , dpx::LE, dpx::Raw_RGB_16_LE },
    { dpx::Raw  , dpx::RGB      , 16, dpx::MethodA, dpx::BE, dpx::Raw_RGB_16_BE },
    { dpx::Raw  , dpx::RGB      , 16, dpx::MethodA, dpx::LE, dpx::Raw_RGB_16_LE },
    { dpx::Raw  , dpx::RGBA     ,  8, dpx::Packed , dpx::BE, dpx::Raw_RGBA_8 },
    { dpx::Raw  , dpx::RGBA     ,  8, dpx::Packed , dpx::LE, dpx::Raw_RGBA_8 },
    { dpx::Raw  , dpx::RGBA     ,  8, dpx::MethodA, dpx::BE, dpx::Raw_RGBA_8 },
    { dpx::Raw  , dpx::RGBA     ,  8, dpx::MethodA, dpx::LE, dpx::Raw_RGBA_8 },
    { dpx::Raw  , dpx::RGBA     , 10, dpx::MethodA, dpx::LE, dpx::Raw_RGBA_10_FilledA_LE },
    { dpx::Raw  , dpx::RGBA     , 10, dpx::MethodA, dpx::BE, dpx::Raw_RGBA_10_FilledA_BE },
    { dpx::Raw  , dpx::RGBA     , 12, dpx::Packed , dpx::BE, dpx::Raw_RGBA_12_Packed_BE },
    { dpx::Raw  , dpx::RGBA     , 12, dpx::MethodA, dpx::BE, dpx::Raw_RGBA_12_FilledA_BE },
    { dpx::Raw  , dpx::RGBA     , 12, dpx::MethodA, dpx::LE, dpx::Raw_RGBA_12_FilledA_LE },
    { dpx::Raw  , dpx::RGBA     , 16, dpx::Packed , dpx::BE, dpx::Raw_RGBA_16_BE },
    { dpx::Raw  , dpx::RGBA     , 16, dpx::Packed , dpx::LE, dpx::Raw_RGBA_16_LE },
    { dpx::Raw  , dpx::RGBA     , 16, dpx::MethodA, dpx::BE, dpx::Raw_RGBA_16_BE },
    { dpx::Raw  , dpx::RGBA     , 16, dpx::MethodA, dpx::LE, dpx::Raw_RGBA_16_LE },
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
    Style(Style_Max),
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
    uint16_t Encoding = Get_X2();
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
            && DPX_Tested_Item.Encoding == Encoding
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
size_t dpx::PixelsPerBlock(dpx::style Style)
{
    switch (Style)
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
dpx::descriptor dpx::ColorSpace(dpx::style Style)
{
    switch (Style)
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
const char* dpx::ColorSpace_String(dpx::style Style)
{
    dpx::descriptor Value = dpx::ColorSpace(Style);

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
uint8_t dpx::BitDepth(dpx::style Style)
{
    switch (Style)
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
const char* dpx::BitDepth_String(dpx::style Style)
{
    uint8_t Value = dpx::BitDepth(Style);

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
dpx::packing dpx::Packing(dpx::style Style)
{
    switch (Style)
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
const char* dpx::Packing_String(dpx::style Style)
{
    dpx::packing Value = dpx::Packing(Style);

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
dpx::endianess dpx::Endianess(dpx::style Style)
{
    switch (Style)
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
const char* dpx::Endianess_String(dpx::style Style)
{
    dpx::endianess Value = dpx::Endianess(Style);

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
string dpx::Flavor_String(dpx::style Style)
{
    string ToReturn("DPX/Raw/");
    ToReturn += ColorSpace_String(Style);
    ToReturn += '/';
    ToReturn += BitDepth_String(Style);
    ToReturn += "bit";
    const char* Value = Packing_String(Style);
    if (Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    Value = Endianess_String(Style);
    if (Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    return ToReturn;
}
