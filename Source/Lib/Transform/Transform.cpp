/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Transform/Transform.h"
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Uncompressed/EXR/EXR.h"
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "Lib/ThirdParty/endianness.h"
//---------------------------------------------------------------------------

//***************************************************************************
// Common
//***************************************************************************

//---------------------------------------------------------------------------
class transform_null : public transform_base
{
public:
    void From(pixel_t*, pixel_t*, pixel_t*, pixel_t*) {}
};

//---------------------------------------------------------------------------
#define JPEG2000RCT(Offset) \
        pixel_t g = y[x]; \
        pixel_t b = u[x]; \
        pixel_t r = v[x]; \
        b -= Offset; \
        r -= Offset; \
        g -= (b + r) >> 2; \
        b += g; \
        r += g; \

//***************************************************************************
// DPX
//***************************************************************************

//---------------------------------------------------------------------------
class transform_dpx : public transform_base
{
public:
    transform_dpx(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t) :
        w(w)
    {
        const auto& Plane = RawFrame->Plane(0);
        FrameBuffer = Plane->Buffer().Data()
            + y_offset * (Plane->BitsPerLine() / 8)
            + x_offset * (Plane->BitsPerBlock() / 8) / Plane->PixelsPerBlock(); //TODO: check when not byte boundary

        NextLine_Offset = (Plane->BitsPerLine() / 8) - w * (Plane->BitsPerBlock() / 8) / Plane->PixelsPerBlock();
    }

    inline void Next()
    {
        FrameBuffer += NextLine_Offset;
    }

protected:
    uint8_t*    FrameBuffer;
    size_t      w;
    size_t      NextLine_Offset;
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_8 : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_8(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 8);
            *(FrameBuffer++) = (uint8_t)r;
            *(FrameBuffer++) = (uint8_t)g;
            *(FrameBuffer++) = (uint8_t)b;
        }

        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_10_FilledA_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_10_FilledA_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 10);
            *(FrameBuffer_Temp_32++) = htol((uint32_t)((r << 22) | (b << 12) | (g << 2))); // Exception indicated in specs, g and b are inverted
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_10_FilledA_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_10_FilledA_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 10);
            *(FrameBuffer_Temp_32++) = htob((uint32_t)((r << 22) | (b << 12) | (g << 2))); // Exception indicated in specs, g and b are inverted
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_12_Packed_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_12_Packed_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            uint32_t Data_Temp;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((b << 24) | (g << 12) | r));
                Data_Temp = (b >> 8);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((b << 28) | (g << 16) | (r << 4) | Data_Temp));
                Data_Temp = (b >> 4);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((g << 20) | (r << 8) | Data_Temp));
                Data_Temp = b;
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((g << 24) | (r << 12) | Data_Temp));
                Data_Temp = (b << 4) | (g >> 8);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((g << 28) | (r << 16) | Data_Temp));
                Data_Temp = (b << 8) | (g >> 4);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((r << 20) | Data_Temp));
                Data_Temp = (b << 12) | g;
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((r << 24) | Data_Temp));
                Data_Temp = (b << 16) | (g << 4) | (r >> 8);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((r << 28) | Data_Temp));
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((b << 20) | (g << 8) | (r >> 4)));
            }
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_12_FilledA_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_12_FilledA_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 12);
            swap(b, g); // Exception indicated in specs, g and b are inverted
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(r << 4));
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(g << 4));
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(b << 4));
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_12_FilledA_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_12_FilledA_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 12);
            swap(b, g); // Exception indicated in specs, g and b are inverted
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(r << 4));
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(g << 4));
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(b << 4));
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_16_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_16_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 16);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)r);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)g);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)b);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_16_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_16_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 16);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)r);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)g);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)b);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_8 : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_8(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 8);
            *(FrameBuffer++) = (uint8_t)r;
            *(FrameBuffer++) = (uint8_t)g;
            *(FrameBuffer++) = (uint8_t)b;
            *(FrameBuffer++) = (uint8_t)a[x];
        }

        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_10_FilledA_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_10_FilledA_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            uint32_t Data_Temp;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htol((uint32_t)((r << 22) | (g << 12) | (b << 2)));
                Data_Temp = (a[x] << 22);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htol((uint32_t)(Data_Temp | (r << 12) | (g << 2)));
                Data_Temp = (b << 22) | (a[x] << 12);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htol((uint32_t)(Data_Temp | (r << 2)));
                *(FrameBuffer_Temp_32++) = htol((uint32_t)((g << 22) | (b << 12) | (a[x] << 2)));
            }
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_10_FilledA_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_10_FilledA_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            uint32_t Data_Temp;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((r << 22) | (g << 12) | (b << 2)));
                Data_Temp = (a[x] << 22);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htob((uint32_t)(Data_Temp | (r << 12) | (g << 2)));
                Data_Temp = (b << 22) | (a[x] << 12);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htob((uint32_t)(Data_Temp | (r << 2)));
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((g << 22) | (b << 12) | (a[x] << 2)));
            }
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_12_Packed_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_12_Packed_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            uint32_t Data_Temp;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((b << 24) | (g << 12) | r));
                Data_Temp = (a[x] << 4) | (b >> 8);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((g << 28) | (r << 16) | (Data_Temp)));
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((a[x] << 20) | (b << 8) | (g >> 4)));
            }
        }
    
        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_12_FilledA_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_12_FilledA_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 12);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(r << 4));
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(g << 4));
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(b << 4));
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(a[x] << 4));
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_12_FilledA_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_12_FilledA_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 12);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(r << 4));
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(g << 4));
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(b << 4));
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(a[x] << 4));
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_16_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_16_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 16);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)r);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)g);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)b);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)a[x]);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_16_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_16_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 16);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)r);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)g);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)b);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)a[x]);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_passthrough_dpx_Raw_Y_8 : public transform_dpx
{
public:
    transform_passthrough_dpx_Raw_Y_8(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t*, pixel_t*, pixel_t*)
    {
        for (size_t x = 0; x < w; x++)
        {
            *(FrameBuffer++) = (int8_t)y[x];
        }

        Next();
    }
};

//---------------------------------------------------------------------------
class transform_passthrough_dpx_Raw_Y_16_LE : public transform_dpx
{
public:
    transform_passthrough_dpx_Raw_Y_16_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t*, pixel_t*, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            *(FrameBuffer_Temp_16++) = htol((uint16_t)y[x]);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_passthrough_dpx_Raw_Y_16_BE : public transform_dpx
{
public:
    transform_passthrough_dpx_Raw_Y_16_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t*, pixel_t*, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            *(FrameBuffer_Temp_16++) = htob((uint16_t)y[x]);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//***************************************************************************
// TIFF
//***************************************************************************

//---------------------------------------------------------------------------
#define SAMEAS(PIX, SOURCE, DEST) \
class transform_##PIX##_##SOURCE : public transform_##PIX##_##DEST \
{ \
public: \
    transform_##PIX##_##SOURCE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) : \
        transform_##PIX##_##DEST(RawFrame, x_offset, y_offset, w, h) {}; \
}; \

//---------------------------------------------------------------------------
SAMEAS(jpeg2000rct, tiff_Raw_RGB_8_U       , dpx_Raw_RGB_8)
SAMEAS(jpeg2000rct, tiff_Raw_RGB_16_U_LE   , dpx_Raw_RGB_16_LE)
SAMEAS(jpeg2000rct, tiff_Raw_RGB_16_U_BE   , dpx_Raw_RGB_16_BE)
SAMEAS(jpeg2000rct, tiff_Raw_RGBA_8_U      , dpx_Raw_RGBA_8)
SAMEAS(jpeg2000rct, tiff_Raw_RGBA_16_U_LE  , dpx_Raw_RGBA_16_LE)
SAMEAS(passthrough, tiff_Raw_Y_8_U         , dpx_Raw_Y_8)
SAMEAS(passthrough, tiff_Raw_Y_16_U_LE     , dpx_Raw_Y_16_LE)
SAMEAS(passthrough, tiff_Raw_Y_16_U_BE     , dpx_Raw_Y_16_BE)

//***************************************************************************
// EXR
//***************************************************************************

//---------------------------------------------------------------------------
class transform_exr : public transform_base
{
public:
    static const size_t Plane_Count = 3; // TODO: handle when not 3 components
    transform_exr(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        w(w)
    {
        const auto& Plane = RawFrame->Plane(0);
        FrameBuffer = Plane->Buffer_Data(0, y_offset); // x_offset span is not supported
        if (x_offset)
        {
            FrameBuffer += x_offset * (Plane->BitsPerBlock() / 8) / Plane->PixelsPerBlock() / Plane_Count;
        }
        else
        {
            auto FrameBuffer_Temp2 = FrameBuffer;
            for (size_t y = 0; y < h; ++y)
            {
                auto* FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer_Temp2;
                FrameBuffer_Temp_32[0] = htol((uint32_t)(y_offset + y));
                FrameBuffer_Temp_32[1] = htol((uint32_t)(Plane->ValidBitsPerLine() / 8));
                FrameBuffer_Temp2 += (Plane->BitsPerLine() / 8);
            }
        }

        FrameBuffer += 8;
        FrameWidth = Plane->Width();
        NextLine_Offset = (Plane->BitsPerLine() / 8) - w * (Plane->BitsPerBlock() / 8) / Plane->PixelsPerBlock() / Plane_Count;
    }

    inline void Next()
    {
        FrameBuffer += NextLine_Offset;
    }

protected:
    uint8_t*    FrameBuffer;
    size_t      w;
    size_t      FrameWidth;
    size_t      NextLine_Offset;
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_exr_Raw_RGB_16 : public transform_exr
{
public:
    transform_jpeg2000rct_exr_Raw_RGB_16(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_exr(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 16);
            FrameBuffer_Temp_16[0] = htol((uint16_t)b);
            FrameBuffer_Temp_16[FrameWidth] = htol((uint16_t)g);
            FrameBuffer_Temp_16[FrameWidth * 2] = htol((uint16_t)r);
            FrameBuffer_Temp_16++;
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//***************************************************************************
// List
//***************************************************************************

//---------------------------------------------------------------------------
#define TRANSFORM_PIX_BEGIN(PIXSTYLE) \
case (pix_style::PIXSTYLE) : \
    switch (RawFrame->Flavor) \
    { \

#define TRANSFORM_PIX_END() \
    default: \
        return new transform_null; \
    } \

#define TRANSFORM_FLAVOR_BEGIN(FORMAT1, FORMAT2) \
case raw_frame::flavor::FORMAT2: \
    switch ((FORMAT1::flavor)RawFrame->Flavor_Private) \
    { \

#define TRANSFORM_FLAVOR_END() \
    default: \
        return new transform_null; \
    } \

#define TRANSFORM_CASE(TRANSFORMSTYLE, FORMAT, FLAVOR) \
case FORMAT::flavor::FLAVOR: \
    return new transform_##TRANSFORMSTYLE##_##FORMAT##_##FLAVOR(RawFrame, x_offset, y_offset, w, h); \

//---------------------------------------------------------------------------
transform_base* Transform_Init(raw_frame* RawFrame, pix_style PixStyle, size_t /*Bits*/, size_t x_offset, size_t y_offset, size_t w, size_t h)
{
    switch (PixStyle)
    {
    TRANSFORM_PIX_BEGIN(RGBA)
        TRANSFORM_FLAVOR_BEGIN(dpx, DPX)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_8)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_10_FilledA_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_10_FilledA_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_12_Packed_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_12_FilledA_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_12_FilledA_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_16_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_16_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_8)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_10_FilledA_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_10_FilledA_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_12_Packed_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_12_FilledA_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_12_FilledA_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_16_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_16_BE)
        TRANSFORM_FLAVOR_END()
        TRANSFORM_FLAVOR_BEGIN(tiff, TIFF)
            TRANSFORM_CASE(jpeg2000rct, tiff, Raw_RGB_8_U)
            TRANSFORM_CASE(jpeg2000rct, tiff, Raw_RGB_16_U_LE)
            TRANSFORM_CASE(jpeg2000rct, tiff, Raw_RGB_16_U_BE)
            TRANSFORM_CASE(jpeg2000rct, tiff, Raw_RGBA_8_U)
            TRANSFORM_CASE(jpeg2000rct, tiff, Raw_RGBA_16_U_LE)
        TRANSFORM_FLAVOR_END()
        TRANSFORM_FLAVOR_BEGIN(exr, EXR)
            TRANSFORM_CASE(jpeg2000rct, exr, Raw_RGB_16)
        TRANSFORM_FLAVOR_END()
    TRANSFORM_PIX_END()
    TRANSFORM_PIX_BEGIN(YUVA)
        TRANSFORM_FLAVOR_BEGIN(dpx, DPX)
            TRANSFORM_CASE(passthrough, dpx, Raw_Y_8)
            TRANSFORM_CASE(passthrough, dpx, Raw_Y_16_LE)
            TRANSFORM_CASE(passthrough, dpx, Raw_Y_16_BE)
        TRANSFORM_FLAVOR_END()
        TRANSFORM_FLAVOR_BEGIN(tiff, TIFF)
            TRANSFORM_CASE(passthrough, tiff, Raw_Y_8_U)
            TRANSFORM_CASE(passthrough, tiff, Raw_Y_16_U_LE)
            TRANSFORM_CASE(passthrough, tiff, Raw_Y_16_U_BE)
        TRANSFORM_FLAVOR_END()
    TRANSFORM_PIX_END()
    default:
        return new transform_null;
    }
}
