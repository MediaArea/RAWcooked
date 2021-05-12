/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef TIFFH
#define TIFFH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include <vector>
#include <set>
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

namespace tiff_issue
{
    namespace undecodable { enum code : uint8_t; }
    namespace unsupported { enum code : uint8_t; }
}

class tiff : public input_base_uncompressed_video
{
public:
    tiff(errors* Errors = NULL);

    // General info
    string                      Flavor_String();
    size_t                      slice_x;
    size_t                      slice_y;

    // Flavors
    ENUM_BEGIN(flavor)
        Raw_RGB_8_U,
        Raw_RGB_16_U_LE,
        Raw_RGB_16_U_BE,
        Raw_RGBA_8_U,
        Raw_RGBA_16_U_LE,
        Raw_Y_8_U,
        Raw_Y_16_U_LE,
        Raw_Y_16_U_BE,
    ENUM_END(flavor)

    // Info about flavors
    static size_t               BytesPerBlock(flavor Flavor);
    static size_t               PixelsPerBlock(flavor Flavor); // Need no overlap every x pixels

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
    void                        Undecodable(tiff_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
    void                        Unsupported(tiff_issue::unsupported::code Code) { input_base::Unsupported((error::unsupported::code)Code); }

    // Specific parsing
    uint32_t                    Get_Element(std::vector<uint32_t>* List = NULL);
    uint32_t                    Get_ElementValue(uint32_t Count, uint32_t Size, std::vector<uint32_t>* List);
    struct data_content
    {
        size_t                  Begin;
        size_t                  End;

        bool operator < (const data_content& Value) const
        {
            return Begin < Value.Begin;
        }
    };
    std::set<data_content>      DataContents;
};

string TIFF_Flavor_String(uint8_t Flavor);

//---------------------------------------------------------------------------
#endif
