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
#include "Lib/Input_Base.h"
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

class dpx : public input_base_uncompressed
{
public:
    dpx();

    bool                        Parse(bool AcceptTruncated = false);
    string                      Flavor_String();

    frame                       Frame;

    // Info
    size_t                      slice_x;
    size_t                      slice_y;
    string*                     FrameRate;

    enum flavor : uint8_t
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
    static size_t BitsPerBlock(flavor Flavor);
    static size_t PixelsPerBlock(flavor Flavor); // Need no overlap every x pixels
    static descriptor ColorSpace(flavor Flavor);
    static const char* ColorSpace_String(flavor Flavor);
    static uint8_t BitDepth(flavor Flavor);
    static const char* BitDepth_String(flavor Flavor);
    static packing Packing(flavor Flavor);
    static const char* Packing_String(flavor Flavor);
    static endianess Endianess(flavor Flavor);
    static const char* Endianess_String(flavor Flavor);
    static string Flavor_String(uint8_t Flavor);
};

//---------------------------------------------------------------------------
#endif
