/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef DPXH
#define DPXH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/FFV1/FFV1_Frame.h"
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

class rawcooked;

class dpx
{
public:
    dpx();

    uint8_t*                    Buffer;
    uint64_t                    Buffer_Size;

    bool                        Parse();

    frame                       Frame;

    rawcooked*                  RAWcooked;

    // Info
    bool                        IsDetected;
    uint8_t                     Style;
    size_t                      slice_x;
    size_t                      slice_y;
    string*                     FrameRate;

    // Error message
    const char*                 ErrorMessage();

    enum style : uint8_t
    {
        Raw_RGB_8,
        Raw_RGB_10_FilledA_LE,
        Raw_RGB_10_FilledA_BE,
        Raw_RGB_12_Packed_BE,
        Raw_RGB_12_FilledA_BE,
        Raw_RGB_12_FilledA_LE,
        Raw_RGB_16_BE,
        Raw_RGB_16_LE,
        Raw_RGBA_8,
        Raw_RGBA_10_FilledA_LE,
        Raw_RGBA_10_FilledA_BE,
        Raw_RGBA_12_Packed_BE,
        Raw_RGBA_12_FilledA_BE,
        Raw_RGBA_12_FilledA_LE,
        Raw_RGBA_16_BE,
        Raw_RGBA_16_LE,
        Style_Max,
    };

    enum endianess : uint8_t
    {
        LE,
        BE,
    };
    enum descriptor : uint8_t
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
    enum encoding : uint8_t
    {
        Raw,
        RLE,
    };
    enum packing : uint8_t
    {
        Packed,
        MethodA,
        MethodB,
    };

    // Info about formats
    static size_t BitsPerBlock(style Style);
    static size_t PixelsPerBlock(style Style); // Need no overlap every x pixels
    static descriptor ColorSpace(style Style);
    static const char* ColorSpace_String(style Style);
    static uint8_t BitDepth(style Style);
    static const char* BitDepth_String(style Style);
    static packing Packing(style Style);
    static const char* Packing_String(style Style);
    static endianess Endianess(style Style);
    static const char* Endianess_String(style Style);
    static string Flavor_String(style Style);

private:
    size_t                      Buffer_Offset;
    bool                        IsBigEndian;
    uint8_t                     Get_X1() { return Buffer[Buffer_Offset++]; }
    uint16_t                    Get_L2();
    uint16_t                    Get_B2();
    uint32_t                    Get_X2() { return IsBigEndian ? Get_B2() : Get_L2(); }
    uint32_t                    Get_L4();
    uint32_t                    Get_B4();
    uint32_t                    Get_X4() { return IsBigEndian ? Get_B4() : Get_L4(); }
    double                      Get_XF4();

    // Error message
    const char*                 error_message;
    bool                        Error(const char* Error) { error_message = Error; return true; }
};

//---------------------------------------------------------------------------
#endif
