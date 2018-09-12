/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef RIFFH
#define RIFFH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/FFV1/FFV1_Frame.h"
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

class rawcooked;

class riff
{
public:
    riff();

    uint8_t*                    Buffer;
    uint64_t                    Buffer_Size;

    bool                        Parse(bool AcceptTruncated=false);

    frame                       Frame;

    rawcooked*                  RAWcooked;

    // Info
    bool                        IsDetected;
    uint8_t                     Style;

    // Error message
    const char*                 ErrorMessage();

    enum style
    {
        PCM_44100_8_1_U,
        PCM_44100_8_2_U,
        PCM_44100_8_6_U,
        PCM_44100_16_1_LE,
        PCM_44100_16_2_LE,
        PCM_44100_16_6_LE,
        PCM_44100_24_1_LE,
        PCM_44100_24_2_LE,
        PCM_44100_24_6_LE,
        PCM_48000_8_1_U,
        PCM_48000_8_2_U,
        PCM_48000_8_6_U,
        PCM_48000_16_1_LE,
        PCM_48000_16_2_LE,
        PCM_48000_16_6_LE,
        PCM_48000_24_1_LE,
        PCM_48000_24_2_LE,
        PCM_48000_24_6_LE,
        PCM_96000_8_1_U,
        PCM_96000_8_2_U,
        PCM_96000_8_6_U,
        PCM_96000_16_1_LE,
        PCM_96000_16_2_LE,
        PCM_96000_16_6_LE,
        PCM_96000_24_1_LE,
        PCM_96000_24_2_LE,
        PCM_96000_24_6_LE,
        Style_Max,
    };
    enum endianess
    {
        BE, // Or Signed for 8-bit
        LE, // Or Unsigned for 8-bit
    };

    // Info about formats
    uint8_t BitDepth();
    endianess Endianess();
    static uint32_t SamplesPerSec(style Style);
    static const char* SamplesPerSec_String(style Style);
    static uint8_t BitDepth(style Style);
    static const char* BitDepth_String(style Style);
    static uint8_t Channels(style Style);
    static const char* Channels_String(style Style);
    static endianess Endianess(style Style);
    static const char* Endianess_String(style Style);
    static string Flavor_String(style Style);

private:
    size_t                      Buffer_Offset;
    uint16_t                    Get_L2();
    uint32_t                    Get_L4();
    uint32_t                    Get_B4();

    typedef void (riff::*call)();
    typedef call(riff::*name)(uint64_t);

    static const size_t Levels_Max = 16;
    struct levels_struct
    {
        name SubElements;
        uint64_t Offset_End;
    };
    levels_struct Levels[Levels_Max];
    size_t Level;
    bool IsList;

#define RIFF_ELEMENT(_NAME) \
        void _NAME(); \
        call SubElements_##_NAME(uint64_t Name);

    RIFF_ELEMENT(_);
    RIFF_ELEMENT(WAVE);
    RIFF_ELEMENT(WAVE_data);
    RIFF_ELEMENT(WAVE_fmt_);
    RIFF_ELEMENT(Void);

    // Error message
    const char*                 error_message;
    bool                        Error(const char* Error) { error_message = Error; return true; }
};

//---------------------------------------------------------------------------
#endif
