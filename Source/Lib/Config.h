/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
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
inline void FormatPath(string& s)
{
    string::size_type i = 0;
    while ((i = s.find('\\', i)) != string::npos)
        s[i++] = '/';
}
#if defined(_WIN32) || defined(_WINDOWS)
    static const char PathSeparator = '\\';
#else
    static const char PathSeparator = '/';
#endif

//---------------------------------------------------------------------------
// Common types - Endianess info
enum class endianness : bool
{
    LE, // Or Unsigned for 8-bit
    BE, // Or Signed for 8-bit
};

//---------------------------------------------------------------------------
// Enums
#define ENUM_BEGIN(_NAME) static const size_t __##_NAME##_line = __LINE__; enum class _NAME : uint8_t {
#define ENUM_END(_NAME) }; static const size_t _NAME##_Max = __LINE__ - __##_NAME##_line - 1;
