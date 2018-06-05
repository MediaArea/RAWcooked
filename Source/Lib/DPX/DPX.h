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
    uint64_t                    Style;
    size_t                      slice_x;
    size_t                      slice_y;
    string*                     FrameRate;

    // Error message
    const char*                 ErrorMessage();

    enum style
    {
        RGB_8,
        RGB_10_FilledA_LE,
        RGB_10_FilledA_BE,
        RGB_12_Packed_BE,
        RGB_12_FilledA_BE,
        RGB_12_FilledA_LE,
        RGB_16_BE,
        RGB_16_LE,
        RGBA_8,
        RGBA_10_FilledA_LE,
        RGBA_10_FilledA_BE,
        RGBA_12_Packed_BE,
        RGBA_12_FilledA_BE,
        RGBA_12_FilledA_LE,
        RGBA_16_BE,
        RGBA_16_LE,
        DPX_Style_Max,
    };

    // Info about formats
    static size_t BitsPerBlock(style Style);
    static size_t PixelsPerBlock(style Style); // Need no overlap every x pixels

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
