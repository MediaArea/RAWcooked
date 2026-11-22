/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef PSDH
#define PSDH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

namespace psd_issue
{
    namespace undecodable { enum code : uint8_t; }
    namespace unsupported { enum code : uint8_t; }
}

// Photoshop document with a raw (uncompressed) merged image
// The merged image has the layout of a planar TIFF (planes after each other, 16-bit big-endian),
// the pixels are handled as the matching TIFF flavor (see TIFF_Flavor())
class psd : public input_base_uncompressed_video
{
public:
    psd(errors* Errors = NULL);

    // General info
    string                      Flavor_String();

    // Flavors
    ENUM_BEGIN(flavor)
        Raw_RGB_8_U,
        Raw_RGB_16_U_BE,
        Raw_Y_8_U,
        Raw_Y_16_U_BE,
    ENUM_END(flavor)

    // Info about flavors
    static tiff::flavor         TIFF_Flavor(flavor Flavor); // TIFF flavor with the same pixel layout

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
    void                        Undecodable(psd_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
    void                        Unsupported(psd_issue::unsupported::code Code) { input_base::Unsupported((error::unsupported::code)Code); }
};

string PSD_Flavor_String(uint8_t Flavor);

//---------------------------------------------------------------------------
#endif
