/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef TransformH
#define TransformH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Common/Common.h"
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
// Direct transform of a line of planar R, G, B, (A) samples (not JPEG2000-RCT coded, e.g. from FFmpeg) to a line of the frame, for some flavors only
// Src: R, G, B, (A) lines, 8-bit samples if Bits is 8 else native endian 16-bit samples
typedef void (*transform_direct)(uint8_t* Dest, const uint8_t* const* Src, size_t w);
transform_direct Transform_Direct_Init(const raw_frame* RawFrame, size_t Bits, bool HasAlpha); // nullptr if not supported

//---------------------------------------------------------------------------
#endif
