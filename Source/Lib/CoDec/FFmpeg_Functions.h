/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFmpeg_FunctionsH
#define FFmpeg_FunctionsH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFmpeg_Library.h"
extern "C"
{
#include <libavcodec/avcodec.h>
}
//---------------------------------------------------------------------------

//***************************************************************************
// Functions of FFmpeg libraries, valid after FFmpeg_Library_Load() returned true
// Called as ffmpeg::Name(), never directly so the libraries can be loaded at run time
//***************************************************************************

// Library, name
#define FFMPEG_FUNCTIONS(_F) \
    _F(avutil,  avutil_version) \
    _F(avcodec, avcodec_version) \

namespace ffmpeg
{
    #define FFMPEG_FUNCTION_DECLARE(_LIBRARY, _NAME) extern decltype(&::_NAME) _NAME;
    FFMPEG_FUNCTIONS(FFMPEG_FUNCTION_DECLARE)
    #undef FFMPEG_FUNCTION_DECLARE
}

//---------------------------------------------------------------------------
#endif
