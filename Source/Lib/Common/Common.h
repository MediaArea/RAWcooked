/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <array>
#include <condition_variable>
#include <cstdint>
#include <cstdint>
#include <cstddef>
#include <string>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Defines the maximum bit size of a pixel
typedef int32_t pixel_t;

//---------------------------------------------------------------------------
// Defines the mode of user interaction

enum user_mode
{
    Ask,
    AlwaysNo,
    AlwaysYes,
};
typedef user_mode(*ask_callback)(user_mode* Mode, const string& FileName, const string& ExtraText, bool Always, bool* ProgressIndicator_IsPaused, condition_variable* ProgressIndicator_IsEnd);


//---------------------------------------------------------------------------
// Hash types
typedef std::array<uint8_t, 16> md5;

//---------------------------------------------------------------------------
// Platform specific
void FormatPath(string& s);
#if defined(_WIN32) || defined(_WINDOWS)
    static const char PathSeparator = '\\';
#else
    static const char PathSeparator = '/';
#endif

//---------------------------------------------------------------------------
// Common types
typedef uint8_t bitdepth;
enum class sign : uint8_t
{
    U, // Unsigned
    S, // Signed
    F, // Float
};
enum class endianness : bool
{
    LE, // Little Endian
    BE, // Big Endian
};
enum class colorspace : uint8_t
{
    RGB,
    RGBA,
    Y,
};
typedef uint8_t channels;
enum class samplerate_code : uint8_t
{
    _44100,
    _48000,
    _96000,
};

//---------------------------------------------------------------------------
// Conversions
samplerate_code SampleRate2Code(uint32_t SampleRate);
size_t Colorspace2Count(colorspace ColorSpace);

//---------------------------------------------------------------------------
// Display
const char* Sign_String(sign Sign);
const char* Endianess_String(endianness Endianness);
const char* ColorSpace_String(colorspace ColorSpace);
const char* SamplesPerSec_String(samplerate_code SampleRateCode);
string BitDepth_String(bitdepth BitDepth);
string Channels_String(channels Channels);
string Raw_Flavor_String(bitdepth BitDepth, sign Sign, endianness Endianness, colorspace ColorSpace);
string PCM_Flavor_String(bitdepth BitDepth, sign Sign, endianness Endianness, channels Channels, samplerate_code SamplesPerSecCode);

//---------------------------------------------------------------------------
// Enums
#define ENUM_BEGIN(_NAME) static const size_t __##_NAME##_line = __LINE__; enum class _NAME : uint8_t {
#define ENUM_END(_NAME) }; static const size_t _NAME##_Max = __LINE__ - __##_NAME##_line - 1;
