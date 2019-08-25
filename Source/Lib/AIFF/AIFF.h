/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef AIFFH
#define AIFFH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/FFV1/FFV1_Frame.h"
#include "Lib/Input_Base.h"
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

namespace aiff_issue
{
    namespace undecodable { enum code : uint8_t; }
    namespace unsupported { enum code : uint8_t; }
}

class aiff : public input_base_uncompressed
{
public:
    aiff(errors* Errors = nullptr);

    string                      Flavor_String();

    enum flavor
    {
        PCM_44100_8_1_S,
        PCM_44100_8_1_U,
        PCM_44100_8_2_S,
        PCM_44100_8_2_U,
        PCM_44100_8_6_S,
        PCM_44100_8_6_U,
        PCM_44100_16_1_BE,
        PCM_44100_16_1_LE,
        PCM_44100_16_2_BE,
        PCM_44100_16_2_LE,
        PCM_44100_16_6_BE,
        PCM_44100_16_6_LE,
        PCM_44100_24_1_BE,
        PCM_44100_24_2_BE,
        PCM_44100_24_6_BE,
        PCM_48000_8_1_S,
        PCM_48000_8_1_U,
        PCM_48000_8_2_S,
        PCM_48000_8_2_U,
        PCM_48000_8_6_S,
        PCM_48000_8_6_U,
        PCM_48000_16_1_BE,
        PCM_48000_16_1_LE,
        PCM_48000_16_2_BE,
        PCM_48000_16_2_LE,
        PCM_48000_16_6_BE,
        PCM_48000_16_6_LE,
        PCM_48000_24_1_BE,
        PCM_48000_24_2_BE,
        PCM_48000_24_6_BE,
        PCM_96000_8_1_S,
        PCM_96000_8_1_U,
        PCM_96000_8_2_S,
        PCM_96000_8_2_U,
        PCM_96000_8_6_S,
        PCM_96000_8_6_U,
        PCM_96000_16_1_BE,
        PCM_96000_16_1_LE,
        PCM_96000_16_2_BE,
        PCM_96000_16_2_LE,
        PCM_96000_16_6_BE,
        PCM_96000_16_6_LE,
        PCM_96000_24_1_BE,
        PCM_96000_24_2_BE,
        PCM_96000_24_6_BE,
        Flavor_Max,
    };
    enum endianness
    {
        BE, // Or Signed for 8-bit
        LE, // Or Unsigned for 8-bit
    };

    // Info about formats
    uint16_t sampleSize();
    endianness Endianness();
    static uint32_t sampleRate(flavor Flavor);
    static const char* sampleRate_String(flavor Flavor);
    static uint16_t sampleSize(flavor Flavor);
    static const char* sampleSize_String(flavor Flavor);
    static uint8_t numChannels(flavor Flavor);
    static const char* numChannels_String(flavor Flavor);
    static endianness Endianness(flavor Flavor);
    static const char* Endianness_String(flavor Flavor);

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
    void                        Undecodable(aiff_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
    void                        Unsupported(aiff_issue::unsupported::code Code) { input_base::Unsupported((error::unsupported::code)Code); }

    typedef void (aiff::*call)();
    typedef call (aiff::*name)(uint64_t);

    static const size_t Levels_Max = 4;
    struct levels_struct
    {
        name SubElements;
        uint64_t Offset_End;
    };
    levels_struct Levels[Levels_Max];
    size_t Level;
    bool IsList;

#define AIFF_EBEMENT(_NAME) \
        void _NAME(); \
        call SubElements_##_NAME(uint64_t Name);

    AIFF_EBEMENT(_);
    AIFF_EBEMENT(AIFC);
    AIFF_EBEMENT(AIFF);
    AIFF_EBEMENT(AIFF_COMM);
    AIFF_EBEMENT(AIFF_SSND);
    AIFF_EBEMENT(Void);
};

string AIFF_Flavor_String(uint8_t Flavor);

//---------------------------------------------------------------------------
#endif
