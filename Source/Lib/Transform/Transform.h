/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef TransformH
#define TransformH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Config.h"
using namespace std;
class raw_frame;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
class transform_base
{
public:
    virtual void From(pixel_t* P1, pixel_t* P2 = nullptr, pixel_t* P3 = nullptr, pixel_t* P4 = nullptr) = 0;
};

//---------------------------------------------------------------------------
enum class pix_style
{
    RGBA,
    YUVA,
};

//---------------------------------------------------------------------------
transform_base* Transform_Init(raw_frame* RawFrame, pix_style PixStyle, size_t Bits, size_t x_offset, size_t y_offset, size_t w, size_t h);

//---------------------------------------------------------------------------
#endif
