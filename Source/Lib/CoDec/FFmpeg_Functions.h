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
#include <libavutil/pixdesc.h>
}
//---------------------------------------------------------------------------

//***************************************************************************
// Functions of FFmpeg libraries, valid after FFmpeg_Library_Load() returned true
// Called as ffmpeg::Name(), never directly so the libraries can be loaded at run time
//***************************************************************************

// Library, name
#define FFMPEG_FUNCTIONS(_F) \
    _F(avutil,  avutil_version) \
    _F(avutil,  av_buffer_create) \
    _F(avutil,  av_buffer_ref) \
    _F(avutil,  av_buffer_unref) \
    _F(avutil,  av_frame_alloc) \
    _F(avutil,  av_frame_free) \
    _F(avutil,  av_frame_unref) \
    _F(avutil,  av_freep) \
    _F(avutil,  av_mallocz) \
    _F(avutil,  av_pix_fmt_desc_get) \
    _F(avutil,  av_read_image_line2) \
    _F(avcodec, avcodec_version) \
    _F(avcodec, av_packet_alloc) \
    _F(avcodec, av_packet_free) \
    _F(avcodec, av_packet_unref) \
    _F(avcodec, avcodec_alloc_context3) \
    _F(avcodec, avcodec_find_decoder) \
    _F(avcodec, avcodec_free_context) \
    _F(avcodec, avcodec_is_open) \
    _F(avcodec, avcodec_open2) \
    _F(avcodec, avcodec_receive_frame) \
    _F(avcodec, avcodec_send_packet) \

namespace ffmpeg
{
    #define FFMPEG_FUNCTION_DECLARE(_LIBRARY, _NAME) extern decltype(&::_NAME) _NAME;
    FFMPEG_FUNCTIONS(FFMPEG_FUNCTION_DECLARE)
    #undef FFMPEG_FUNCTION_DECLARE
}

//---------------------------------------------------------------------------
#endif
