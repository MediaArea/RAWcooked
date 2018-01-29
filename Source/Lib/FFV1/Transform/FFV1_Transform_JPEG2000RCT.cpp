/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/FFV1/Transform/FFV1_Transform_JPEG2000RCT.h"
#include "Lib/DPX/DPX.h"
#include "Lib/RawFrame/RawFrame.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
transform_jpeg2000rct::transform_jpeg2000rct(raw_frame* RawFrame_, size_t Bits_, size_t y_offset, size_t x_offset) :
    RawFrame(RawFrame_),
    Bits(Bits_)
{
    Offset = ((pixel_t)1) << Bits;
    Style_Private = RawFrame->Style_Private;

    size_t FrameBuffer_Temp_Size = RawFrame->Planes.size();
    for (size_t p = 0; p<FrameBuffer_Temp_Size; p++)
    {
        FrameBuffer_Temp[p] = RawFrame->Planes[p]->Buffer;
        FrameBuffer_Temp[p] += y_offset*RawFrame->Planes[p]->AllBytesPerLine();
        FrameBuffer_Temp[p] += x_offset*RawFrame->Planes[p]->BytesPerPixel;
    }
}

//---------------------------------------------------------------------------
void transform_jpeg2000rct::From(size_t w, pixel_t* Y, pixel_t* U, pixel_t* V, pixel_t* A)
{
    switch (RawFrame->Style)
    {
        case raw_frame::Style_FFmpeg: FFmpeg_From(w, Y, U, V, A); break;
        case raw_frame::Style_DPX: DPX_From(w, Y, U, V, A); break;
        default:;
    }

    for (size_t p = 0; p<RawFrame->Planes.size(); p++)
        FrameBuffer_Temp[p]+= RawFrame->Planes[p]->AllBytesPerLine();
}

//---------------------------------------------------------------------------
void transform_jpeg2000rct::FFmpeg_From(size_t w, pixel_t* Y, pixel_t* U, pixel_t* V, pixel_t* A)
{
    for (size_t x = 0; x < w; ++x)
    {
        pixel_t g = Y[x];
        pixel_t b = U[x];
        pixel_t r = V[x];
        pixel_t a = 0;
        if (A)
            a = A[x];

        b -= Offset;
        r -= Offset;
        g -= (b + r) >> 2;
        b += g;
        r += g;

        if (Bits <= 8)
            ((uint32_t*)FrameBuffer_Temp[0])[x] = b + (g << 8) + (r << 16) + (a << 24);
        else if (Bits >= 16 && !A)
        {
            ((uint16_t*)FrameBuffer_Temp[0])[x] = g;
            ((uint16_t*)FrameBuffer_Temp[1])[x] = b;
            ((uint16_t*)FrameBuffer_Temp[2])[x] = r;
        }
        else
        {
            ((uint16_t*)FrameBuffer_Temp[0])[x] = b;
            ((uint16_t*)FrameBuffer_Temp[1])[x] = g;
            ((uint16_t*)FrameBuffer_Temp[2])[x] = r;
            if (A)
                ((uint16_t*)FrameBuffer_Temp[3])[x] = a;
        }
    }
}

//---------------------------------------------------------------------------
void transform_jpeg2000rct::DPX_From(size_t w, pixel_t* Y, pixel_t* U, pixel_t* V, pixel_t* A)
{
    uint8_t*  FrameBuffer_Temp_8  = (uint8_t* )FrameBuffer_Temp[0];
    uint16_t* FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer_Temp[0];
    uint32_t* FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer_Temp[0];

    for (size_t x = 0; x < w; x++)
    {
        pixel_t g = Y[x];
        pixel_t b = U[x];
        pixel_t r = V[x];

        b -= Offset;
        r -= Offset;
        g -= (b + r) >> 2;
        b += g;
        r += g;

        switch (Style_Private)
        {
            case dpx::RGB_10_MethodA_LE:
                                        {
                                        uint32_t c;
                                        if (Bits >= 16 && !A)
                                            c = (r << 22) | (g << 12) | (b << 2);
                                        else
                                            c = (r << 22) | (b << 12) | (g << 2); // Exception indicated in specs
                                        FrameBuffer_Temp_32[x] = c;
                                        }
                                        break;
            case dpx::RGB_10_MethodA_BE:
                                        {
                                        uint32_t c;
                                        if (Bits >= 16 && !A)
                                            c = (r << 22) | (g << 12) | (b << 2);
                                        else
                                            c = (r << 22) | (b << 12) | (g << 2); // Exception indicated in specs
                                        FrameBuffer_Temp_32[x] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                        }
                                        break;
            case dpx::RGB_16_BE:
                                        {
                                        FrameBuffer_Temp_16[x*3]   = ((r & 0xFF00) >> 8) | ((r & 0xFF) << 8);  // Swap bytes
                                        FrameBuffer_Temp_16[x*3+1] = ((g & 0xFF00) >> 8) | ((g & 0xFF) << 8);  // Swap bytes
                                        FrameBuffer_Temp_16[x*3+2] = ((b & 0xFF00) >> 8) | ((b & 0xFF) << 8);  // Swap bytes
                                        }
                                        break;
            case dpx::RGBA_8:
                                        {
                                        pixel_t a = A[x];
                                        FrameBuffer_Temp_8[x*4]   = (uint8_t)r;
                                        FrameBuffer_Temp_8[x*4+1] = (uint8_t)g;
                                        FrameBuffer_Temp_8[x*4+2] = (uint8_t)b;
                                        FrameBuffer_Temp_8[x*4+3] = (uint8_t)a;
                                        }
                                        break;
            case dpx::RGBA_16_BE:
                                        {
                                        pixel_t a = A[x];
                                        FrameBuffer_Temp_16[x*4]   = ((r & 0xFF00) >> 8) | ((r & 0xFF) << 8);  // Swap bytes
                                        FrameBuffer_Temp_16[x*4+1] = ((g & 0xFF00) >> 8) | ((g & 0xFF) << 8);  // Swap bytes
                                        FrameBuffer_Temp_16[x*4+2] = ((b & 0xFF00) >> 8) | ((b & 0xFF) << 8);  // Swap bytes
                                        FrameBuffer_Temp_16[x*4+3] = ((a & 0xFF00) >> 8) | ((a & 0xFF) << 8);  // Swap bytes
                                        }
                                        break;
            default:;
        }
    }
}
