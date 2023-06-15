/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef AVIH
#define AVIH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

namespace avi_issue
{
    namespace undecodable { enum code : uint8_t; }
    namespace unsupported { enum code : uint8_t; }
}

class avi : public input_base_uncompressed_compound
{
public:
    avi(errors* Errors = NULL);

    // General info
    string                      Flavor_String();
    uint8_t                     BitDepth();
    sign                        Sign();
    endianness                  Endianness();
    size_t                      slice_x;
    size_t                      slice_y;

    ENUM_BEGIN(flavor)
        v210,
    ENUM_END(flavor)

    // Info about formats
    size_t                      GetStreamCount();

    // Info about flavors
    static size_t               BytesPerBlock(flavor Flavor);
    static size_t               PixelsPerBlock(flavor Flavor); // Need no overlap every x pixels

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
    void                        Undecodable(avi_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
    void                        Unsupported(avi_issue::unsupported::code Code) { input_base::Unsupported((error::unsupported::code)Code); }
    typedef void (avi::*call)();
    typedef call (avi::*name)(uint64_t);

    static const size_t         Levels_Max = 5;
    struct levels_struct
    {
        name                    SubElements;
        uint64_t                Offset_End;
    };
    levels_struct               Levels[Levels_Max];
    size_t                      Level;
    bool                        IsList;

    // Temp
    uint16_t                    Width;
    uint16_t                    Height;
    uint32_t                    BlockAlign = 0;
    uint8_t*                    In = nullptr;
    size_t                      In_Pos = 0;
    size_t                      Buffer_LastPos = 0;
    struct track
    {
        uint32_t                fccType = 0;
        uint32_t                fccHandler = 0;
    };
    vector<track>               Tracks;

#define AVI_ELEMENT(_NAME) \
        void _NAME(); \
        call SubElements_##_NAME(uint64_t Name);

    AVI_ELEMENT(_);
    AVI_ELEMENT(AVI_);
    AVI_ELEMENT(AVI__hdrl);
    AVI_ELEMENT(AVI__hdrl_strl);
    AVI_ELEMENT(AVI__hdrl_strl_strf);
    AVI_ELEMENT(AVI__hdrl_strl_strh);
    AVI_ELEMENT(AVI__movi);
    AVI_ELEMENT(AVI__movi_00db);
    AVI_ELEMENT(AVI__movi_00dc);
    AVI_ELEMENT(AVI__movi_01wb);
    AVI_ELEMENT(AVIX);
    AVI_ELEMENT(AVIX_movi);
    AVI_ELEMENT(AVIX_movi_00db);
    AVI_ELEMENT(AVIX_movi_00dc);
    AVI_ELEMENT(AVIX_movi_01wb);
    AVI_ELEMENT(Void);

#undef AVI_ELEMENT

    void AVI__hdrl_strl_strf_vids();
    void AVI__hdrl_strl_strf_auds();
};

string AVI_Flavor_String(uint8_t Flavor);

//---------------------------------------------------------------------------
#endif
