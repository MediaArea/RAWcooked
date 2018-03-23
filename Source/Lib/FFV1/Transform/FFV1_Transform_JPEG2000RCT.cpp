/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
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
        FrameBuffer_Temp[p] += x_offset*RawFrame->Planes[p]->BitsPerBlock/RawFrame->Planes[p]->PixelsPerBlock/8; //TODO: check when not byte boundary
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

    size_t t=0, s=0;
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
            case dpx::RGB_8:
                                        {
                                        FrameBuffer_Temp_8[x*3]   = (uint8_t)r;
                                        FrameBuffer_Temp_8[x*3+1] = (uint8_t)g;
                                        FrameBuffer_Temp_8[x*3+2] = (uint8_t)b;
                                        }
                                        break;
            case dpx::RGB_10_FilledA_LE:
                                        {
                                        FrameBuffer_Temp_32[x] = (r << 22) | (b << 12) | (g << 2); // Exception indicated in specs, g and b are inverted
                                        }
                                        break;
            case dpx::RGB_10_FilledA_BE:
                                        {
                                        uint32_t c = (r << 22) | (b << 12) | (g << 2); // Exception indicated in specs, g and b are inverted
                                        FrameBuffer_Temp_32[x] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                        }
                                        break;
            case dpx::RGB_12_Packed_BE:
                                        {
                                        uint32_t c; // Exception indicated in specs, g and b are inverted
                                        swap(b, g);
                                        switch (s)
                                        {
                                            case 0:
                                                    c = (b << 24) | (g << 12) | r;
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    Data_Private = (b >> 8);
                                                    break;
                                            case 1:
                                                    c = (b << 28) | (g << 16) | (r << 4) | (uint32_t)Data_Private;
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    Data_Private = (b >> 4);
                                                    break;
                                            case 2:
                                                    c = (g << 20) | (r << 8) | (uint32_t)Data_Private;
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    Data_Private = b;
                                                    break;
                                            case 3:
                                                    c = (g << 24) | (r << 12) | (uint32_t)Data_Private;
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    Data_Private = (b << 4) | (g >> 8);
                                                    break;
                                            case 4:
                                                    c = (g << 28) | (r << 16) | (uint32_t)Data_Private;
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    Data_Private = (b << 8) | (g >> 4);
                                                    break;
                                            case 5:
                                                    c = (r << 20) | (uint32_t)Data_Private;
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    Data_Private = (b << 12) | (g);
                                                    break;
                                            case 6:
                                                    c = (r << 24) | (uint32_t)Data_Private;
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    Data_Private = (b << 16) | (g << 4) | (r >> 8);
                                                    break;
                                            case 7:
                                                    c = (r << 28) | (uint32_t)Data_Private;
                                                    FrameBuffer_Temp_32[t++] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    c = (b << 20) | (g << 8) | (r >> 4);
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    break;
                                        }
                                        t++;
                                        s++;
                                        if (s == 8)
                                            s = 0;
                                        }
                                        break;
            case dpx::RGB_12_FilledA_BE:
                                        {  // Exception indicated in specs, g and b are inverted
                                        FrameBuffer_Temp_16[x*3]   = ((r & 0xFF0) >> 4) | ((r & 0xF) << 12);  // Swap bytes after shift by 4
                                        FrameBuffer_Temp_16[x*3+1] = ((b & 0xFF0) >> 4) | ((b & 0xF) << 12);  // Swap bytes after shift by 4
                                        FrameBuffer_Temp_16[x*3+2] = ((g & 0xFF0) >> 4) | ((g & 0xF) << 12);  // Swap bytes after shift by 4
                                        }
                                        break;
            case dpx::RGB_12_FilledA_LE:
                                        {  // Exception indicated in specs, g and b are inverted
                                        FrameBuffer_Temp_16[x*3]   = r << 4;
                                        FrameBuffer_Temp_16[x*3+1] = b << 4;
                                        FrameBuffer_Temp_16[x*3+2] = g << 4;
                                        }
                                        break;
            case dpx::RGB_16_BE:
                                        { 
                                        FrameBuffer_Temp_16[x*3]   = ((r & 0xFF00) >> 8) | ((r & 0xFF) << 8);  // Swap bytes
                                        FrameBuffer_Temp_16[x*3+1] = ((g & 0xFF00) >> 8) | ((g & 0xFF) << 8);  // Swap bytes
                                        FrameBuffer_Temp_16[x*3+2] = ((b & 0xFF00) >> 8) | ((b & 0xFF) << 8);  // Swap bytes
                                        }
                                        break;
            case dpx::RGB_16_LE:
                                        { 
                                        FrameBuffer_Temp_16[x*3]   = r;
                                        FrameBuffer_Temp_16[x*3+1] = g;
                                        FrameBuffer_Temp_16[x*3+2] = b;
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
            case dpx::RGBA_10_FilledA_BE:
                                        {
                                        pixel_t a = A[x];
                                        uint32_t c;
                                        switch (s)
                                        {
                                            case 0:
                                                    c = (r << 22) | (g << 12) | (b << 2);
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    Data_Private = (a << 22);
                                                    break;
                                            case 1:
                                                    c = (uint32_t)Data_Private | (r << 12) | (g << 2);
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    Data_Private = (b << 22) | (a << 12);
                                                    break;
                                            case 2:
                                                    c = (uint32_t)Data_Private | (r << 2);
                                                    FrameBuffer_Temp_32[t++] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    c = (g << 22) | (b << 12) | (a << 2);
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    break;
                                        }
                                        t++;
                                        s++;
                                        if (s == 3)
                                            s = 0;
                                        }
                                        break;
            case dpx::RGBA_10_FilledA_LE:
                                        {
                                        pixel_t a = A[x];
                                        switch (s)
                                        {
                                            case 0:
                                                    FrameBuffer_Temp_32[t] = (r << 22) | (g << 12) | (b << 2);
                                                    Data_Private = (a << 22);
                                                    break;
                                            case 1:
                                                    FrameBuffer_Temp_32[t] = (uint32_t)Data_Private | (r << 12) | (g << 2);
                                                    Data_Private = (b << 22) | (a << 12);
                                                    break;
                                            case 2:
                                                    FrameBuffer_Temp_32[t++] = (uint32_t)Data_Private | (r << 2);
                                                    FrameBuffer_Temp_32[t] = (g << 22) | (b << 12) | (a << 2);
                                                    break;
                                        }
                                        t++;
                                        s++;
                                        if (s == 3)
                                            s = 0;
                                        }
                                        break;
            case dpx::RGBA_12_Packed_BE:
                                        {
                                        pixel_t a = A[x];
                                        uint32_t c;
                                        switch (s)
                                        {
                                            case 0:
                                                    c = (b << 24) | (g << 12) | r;
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    Data_Private = (a << 4) | (b >> 8);
                                                    break;
                                            case 1:
                                                    c = (g << 28) | (r << 16) | (uint32_t)Data_Private;
                                                    FrameBuffer_Temp_32[t++] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    c = (a << 20) | (b << 8) | (g >> 4);
                                                    FrameBuffer_Temp_32[t] = ((c & 0xFF000000) >> 24) | ((c & 0x00FF0000) >> 8) | ((c & 0x0000FF00) << 8) | ((c & 0x000000FF) << 24); // Swap bytes
                                                    break;
                                        }
                                        t++;
                                        s++;
                                        if (s == 2)
                                            s = 0;
                                        }
                                        break;
            case dpx::RGBA_12_FilledA_BE:
                                        {
                                        pixel_t a = A[x];
                                        FrameBuffer_Temp_16[x*4]   = (uint16_t)((r & 0xFF0) >> 4) | ((r & 0xF) << 12);  // Swap bytes after shift by 4
                                        FrameBuffer_Temp_16[x*4+1] = (uint16_t)((g & 0xFF0) >> 4) | ((g & 0xF) << 12);  // Swap bytes after shift by 4
                                        FrameBuffer_Temp_16[x*4+2] = (uint16_t)((b & 0xFF0) >> 4) | ((b & 0xF) << 12);  // Swap bytes after shift by 4
                                        FrameBuffer_Temp_16[x*4+3] = (uint16_t)((a & 0xFF0) >> 4) | ((a & 0xF) << 12);  // Swap bytes after shift by 4
                                        }
                                        break;
            case dpx::RGBA_12_FilledA_LE:
                                        {
                                        pixel_t a = A[x];
                                        FrameBuffer_Temp_16[x*4]   = r << 4;
                                        FrameBuffer_Temp_16[x*4+1] = g << 4;
                                        FrameBuffer_Temp_16[x*4+2] = b << 4;
                                        FrameBuffer_Temp_16[x*4+3] = a << 4;
                                        }
                                        break;
            case dpx::RGBA_16_BE:
                                        {
                                        pixel_t a = A[x];
                                        FrameBuffer_Temp_16[x*4]   = (uint16_t)((r & 0xFF00) >> 8) | ((r & 0xFF) << 8);  // Swap bytes
                                        FrameBuffer_Temp_16[x*4+1] = (uint16_t)((g & 0xFF00) >> 8) | ((g & 0xFF) << 8);  // Swap bytes
                                        FrameBuffer_Temp_16[x*4+2] = (uint16_t)((b & 0xFF00) >> 8) | ((b & 0xFF) << 8);  // Swap bytes
                                        FrameBuffer_Temp_16[x*4+3] = (uint16_t)((a & 0xFF00) >> 8) | ((a & 0xFF) << 8);  // Swap bytes
                                        }
                                        break;
            case dpx::RGBA_16_LE:
                                        {
                                        pixel_t a = A[x];
                                        FrameBuffer_Temp_16[x*4]   = r;
                                        FrameBuffer_Temp_16[x*4+1] = g;
                                        FrameBuffer_Temp_16[x*4+2] = b;
                                        FrameBuffer_Temp_16[x*4+3] = a;
                                        }
                                        break;
            default:;
        }
    }
}
