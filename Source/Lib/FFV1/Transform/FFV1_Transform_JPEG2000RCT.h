/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef Transform_JPEG2000RCTH
#define Transform_JPEG2000RCTH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Config.h"
using namespace std;
//---------------------------------------------------------------------------

class raw_frame;

class transform_jpeg2000rct
{
public:
    transform_jpeg2000rct(raw_frame* RawFrame, size_t Bits, size_t y_offset, size_t x_offset);

    void From(size_t w, pixel_t* Y, pixel_t* U, pixel_t* V, pixel_t* A);

private:
    raw_frame*                  RawFrame;
    uint8_t*                    FrameBuffer_Temp[4];
    size_t                      Bits;
    pixel_t                     Offset;
    uint64_t                    Style_Private; //Used by specialized style for marking the configuration of such style (e.g. endianess of DPX)

    void FFmpeg_From(size_t w, pixel_t* Y, pixel_t* U, pixel_t* V, pixel_t* A);
    void DPX_From(size_t w, pixel_t* Y, pixel_t* U, pixel_t* V, pixel_t* A);
};

//---------------------------------------------------------------------------
#endif
