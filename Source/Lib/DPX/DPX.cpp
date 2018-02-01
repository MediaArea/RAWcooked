/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/DPX/DPX.h"
#include "Lib/RAWcooked/RAWcooked.h"
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
    uint8_t                     Code;
};

struct dpx_tested DPX_Tested[] =
{
    { RGB       , 10, MethodA, LE, dpx::RGB_10_FilledA_LE },
    { RGB       , 10, MethodA, BE, dpx::RGB_10_FilledA_BE },
    { RGB       , 16, Packed , BE, dpx::RGB_16_BE },
    { RGBA      ,  8, Packed , BE, dpx::RGBA_8 },
    { RGBA      , 16, MethodA, BE, dpx::RGBA_16_BE },
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
    WriteFileCall(NULL),
    WriteFileCall_Opaque(NULL),
    Style(DPX_Style_Max),
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
bool dpx::Parse()
{
    if (Buffer_Size < 1664)
        return Error("DPX file size");

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
            return Error("DPX header");
    }
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
    Buffer_Offset = 660;
    uint32_t Encryption = Get_X4();
    if (Encryption != 0xFFFFFFFF && Encryption != 0) // One file found with Encryption of 0 but not encrypted, we accept it.
        return Error("Encryption key");
    Buffer_Offset = 768;
    if (Get_X2() != 0)
        return Error("Image orientation");
    if (Get_X2() != 1)
        return Error("Number of image elements");
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
    uint32_t EndOfImagePadding = Get_X4();

    // Supported?
    Style = 0;
    for (; Style < DPX_Style_Max; Style++)
    {
        dpx_tested& DPX_Tested_Item = DPX_Tested[Style];
        if (DPX_Tested_Item.Descriptor == Descriptor
         && DPX_Tested_Item.BitDepth == BitDepth
         && DPX_Tested_Item.Packing == Packing
         && DPX_Tested_Item.Endianess == (IsBigEndian?BE:LE))
            break;
    }
    if (Style >= DPX_Style_Max)
        return Error("Style (Descriptor / BitDepth / Packing / Endianess combination)");

    // Write RAWcooked file
    if (WriteFileCall)
    {
        rawcooked RAWcooked;
        RAWcooked.WriteFileCall = WriteFileCall;
        RAWcooked.WriteFileCall_Opaque = WriteFileCall_Opaque;
        RAWcooked.Before = Buffer;
        RAWcooked.Before_Size = OffsetToImage;
        RAWcooked.After = Buffer + Buffer_Size - EndOfImagePadding;
        RAWcooked.After_Size = EndOfImagePadding;
        RAWcooked.Parse();
    }

    return 0;
}
