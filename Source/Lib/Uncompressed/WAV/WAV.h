/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef WAVH
#define WAVH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

namespace wav_issue
{
    namespace undecodable { enum code : uint8_t; }
    namespace unsupported { enum code : uint8_t; }
}

class wav : public input_base_uncompressed_audio
{
public:
    wav(errors* Errors = NULL);

    // General info
    string                      Flavor_String();
    uint8_t                     BitDepth();
    sign                        Sign();
    endianness                  Endianness();
    uint8_t                     Channels();

    ENUM_BEGIN(flavor)
        PCM_44100_8_1_U,
        PCM_44100_8_2_U,
        PCM_44100_8_4_U,
        PCM_44100_8_6_U,
        PCM_44100_8_8_U,
        PCM_44100_16_1_LE,
        PCM_44100_16_2_LE,
        PCM_44100_16_4_LE,
        PCM_44100_16_6_LE,
        PCM_44100_16_8_LE,
        PCM_44100_24_1_LE,
        PCM_44100_24_2_LE,
        PCM_44100_24_4_LE,
        PCM_44100_24_6_LE,
        PCM_44100_24_8_LE,
        PCM_44100_32_1_LE,
        PCM_44100_32_1_LE_F,
        PCM_44100_32_2_LE,
        PCM_44100_32_2_LE_F,
        PCM_44100_32_4_LE,
        PCM_44100_32_4_LE_F,
        PCM_44100_32_6_LE,
        PCM_44100_32_6_LE_F,
        PCM_44100_32_8_LE,
        PCM_44100_32_8_LE_F,
        PCM_48000_8_1_U,
        PCM_48000_8_2_U,
        PCM_48000_8_4_U,
        PCM_48000_8_6_U,
        PCM_48000_8_8_U,
        PCM_48000_16_1_LE,
        PCM_48000_16_2_LE,
        PCM_48000_16_4_LE,
        PCM_48000_16_6_LE,
        PCM_48000_16_8_LE,
        PCM_48000_24_1_LE,
        PCM_48000_24_2_LE,
        PCM_48000_24_4_LE,
        PCM_48000_24_6_LE,
        PCM_48000_24_8_LE,
        PCM_48000_32_1_LE,
        PCM_48000_32_1_LE_F,
        PCM_48000_32_2_LE,
        PCM_48000_32_2_LE_F,
        PCM_48000_32_4_LE,
        PCM_48000_32_4_LE_F,
        PCM_48000_32_6_LE,
        PCM_48000_32_6_LE_F,
        PCM_48000_32_8_LE,
        PCM_48000_32_8_LE_F,
        PCM_96000_8_1_U,
        PCM_96000_8_2_U,
        PCM_96000_8_4_U,
        PCM_96000_8_6_U,
        PCM_96000_8_8_U,
        PCM_96000_16_1_LE,
        PCM_96000_16_2_LE,
        PCM_96000_16_4_LE,
        PCM_96000_16_6_LE,
        PCM_96000_16_8_LE,
        PCM_96000_24_1_LE,
        PCM_96000_24_2_LE,
        PCM_96000_24_4_LE,
        PCM_96000_24_6_LE,
        PCM_96000_24_8_LE,
        PCM_96000_32_1_LE,
        PCM_96000_32_1_LE_F,
        PCM_96000_32_2_LE,
        PCM_96000_32_2_LE_F,
        PCM_96000_32_4_LE,
        PCM_96000_32_4_LE_F,
        PCM_96000_32_6_LE,
        PCM_96000_32_6_LE_F,
        PCM_96000_32_8_LE,
        PCM_96000_32_8_LE_F,
    ENUM_END(flavor)

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
    void                        Undecodable(wav_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
    void                        Unsupported(wav_issue::unsupported::code Code) { input_base::Unsupported((error::unsupported::code)Code); }
    typedef void (wav::*call)();
    typedef call (wav::*name)(uint64_t);

    static const size_t         Levels_Max = 4;
    struct levels_struct
    {
        name                    SubElements;
        uint64_t                Offset_End;
    };
    levels_struct               Levels[Levels_Max];
    size_t                      Level;
    bool                        IsList;

    // Temp
    uint64_t                    ds64_riffOffset = 0;
    uint64_t                    ds64_dataSize = 0;
    uint32_t                    BlockAlign = 0;
    bool                        IsRF64 = false;
    bool                        DataIsParsed = false;

#define WAV_ELEMENT(_NAME) \
        void _NAME(); \
        call SubElements_##_NAME(uint64_t Name);

    WAV_ELEMENT(_);
    WAV_ELEMENT(WAVE);
    WAV_ELEMENT(WAVE_data);
    WAV_ELEMENT(WAVE_ds64);
    WAV_ELEMENT(WAVE_fmt_);
    WAV_ELEMENT(Void);
};

string              WAV_Flavor_String(uint8_t Flavor);
uint8_t             WAV_BitDepth(wav::flavor Flavor);
sign                WAV_Sign(wav::flavor Flavor);
endianness          WAV_Endianness(wav::flavor Flavor);
uint8_t             WAV_Channels(wav::flavor Flavor);

//---------------------------------------------------------------------------
#endif
