/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef EXRH
#define EXRH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Utils/FileIO/Input_Base.h"
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

namespace exr_issue
{
    namespace undecodable { enum code : uint8_t; }
    namespace unsupported { enum code : uint8_t; }
    namespace invalid     { enum code : uint8_t; }
}

class exr : public input_base_uncompressed_video
{
public:
    exr(errors* Errors = nullptr);
    ~exr();

    // General info
    string                      Flavor_String();
    size_t                      slice_x;
    size_t                      slice_y;

    // Flavors
    ENUM_BEGIN(flavor)
        Raw_RGB_16,
    ENUM_END(flavor)

    // Info about flavors
    static size_t               BytesPerBlock(flavor Flavor);
    static size_t               PixelsPerBlock(flavor Flavor); // Need no overlap every x pixels

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
    void                        Undecodable(exr_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
    void                        Unsupported(exr_issue::unsupported::code Code) { input_base::Unsupported((error::unsupported::code)Code); }
    void                        Invalid(exr_issue::invalid::code Code) { input_base::Invalid((error::invalid::code)Code); }
};

string EXR_Flavor_String(uint8_t Flavor);

//---------------------------------------------------------------------------
#endif
