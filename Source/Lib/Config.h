/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
*
*  Use of this source code is governed by a BSD-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstdint>
#include <cstddef>
#include <string>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Defines the maximum bit size of a pixel
typedef int32_t pixel_t;

//---------------------------------------------------------------------------
// Platform specific
#if defined(_WIN32) || defined(_WINDOWS)
    static const char PathSeparator = '\\';
#else
    static const char PathSeparator = '/';
#endif
