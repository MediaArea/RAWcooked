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
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

namespace dpx_issue
{
    namespace undecodable { enum code : uint8_t; }
    namespace unsupported { enum code : uint8_t; }
    namespace invalid     { enum code : uint8_t; }
}

class dpx : public input_base_uncompressed_video
{
public:
    dpx(errors* Errors = nullptr);
    ~dpx();

    // General info
    string                      Flavor_String();
    size_t                      slice_x;
    size_t                      slice_y;

    // Flavors
    ENUM_BEGIN(flavor)
        Raw_RGB_8,
        Raw_RGB_10_FilledA_LE,
        Raw_RGB_10_FilledA_BE,
        Raw_RGB_12_Packed_BE,
        Raw_RGB_12_FilledA_LE,
        Raw_RGB_12_FilledA_BE,
        Raw_RGB_16_LE,
        Raw_RGB_16_BE,
        Raw_RGBA_8,
        Raw_RGBA_10_FilledA_LE,
        Raw_RGBA_10_FilledA_BE,
        Raw_RGBA_12_Packed_BE,
        Raw_RGBA_12_FilledA_LE,
        Raw_RGBA_12_FilledA_BE,
        Raw_RGBA_16_LE,
        Raw_RGBA_16_BE,
    ENUM_END(flavor)

    // Features
    bool                        MayHavePaddingBits();

    // Info about flavors
    static size_t               BytesPerBlock(flavor Flavor);
    static size_t               PixelsPerBlock(flavor Flavor); // Need no overlap every x pixels

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
    void                        ConformanceCheck();
    void                        Undecodable(dpx_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
    void                        Unsupported(dpx_issue::unsupported::code Code) { input_base::Unsupported((error::unsupported::code)Code); }
    void                        Invalid(dpx_issue::invalid::code Code) { input_base::Invalid((error::invalid::code)Code); }

    // Comparison
    uint8_t*                    HeaderCopy = NULL;
    uint64_t                    HeaderCopy_Info; // 0-11: buffer size - 1, 12: ignore offsets to data image 
};

string DPX_Flavor_String(uint8_t Flavor);

//---------------------------------------------------------------------------
#endif
