/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/RawFrame/RawFrame.h"
#include "Lib/DPX/DPX.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void raw_frame::Create(style Style_, size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample)
{
    if (!Planes.empty())
        return; //TODO: manage when it changes

    Style = Style_;

    for (size_t i = 0; i < Planes.size(); i++)
        delete Planes[i];
    Planes.clear();

    switch (Style)
    {
        case Style_FFmpeg: FFmpeg_Create(colorspace_type, width, height, bits_per_raw_sample, chroma_planes, alpha_plane, h_chroma_subsample, v_chroma_subsample); break;
        case Style_DPX: DPX_Create(colorspace_type, width, height, bits_per_raw_sample, chroma_planes, alpha_plane, h_chroma_subsample, v_chroma_subsample); break;
    }
}

//---------------------------------------------------------------------------
void raw_frame::FFmpeg_Create(size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample)
{
    switch (colorspace_type)
    {
        case 0: // YCbCr --> YA Packed if no chroma, alpha and <=8, else YUV Planar
                Planes.push_back(new plane(width, height, (bits_per_raw_sample + 7) / 8 * ((bits_per_raw_sample <= 8 && !chroma_planes && alpha_plane) ? 2 : 1))); // Luma
                if (chroma_planes)
                {
                    size_t h_divisor = h_chroma_subsample;
                    size_t v_divisor = v_chroma_subsample;
                    for (size_t i = 0; i < 2; i++)
                        Planes.push_back(new plane((width + h_divisor - 1) / h_divisor, (height + v_divisor - 1) / v_divisor, bits_per_raw_sample / 8 + ((bits_per_raw_sample % 8) ? 1 : 0))); //Chroma
                }
                if (alpha_plane && !(bits_per_raw_sample <= 8 && !chroma_planes))
                    Planes.push_back(new plane(width, height, (bits_per_raw_sample + 7) / 8)); //Alpha
                break;
        case 1: // JPEG2000-RCT --> RGB Packed if <=8, else RGB planar
                if (bits_per_raw_sample <= 8)
                    Planes.push_back(new plane(width, height, (bits_per_raw_sample + 7) / 8 * 4)); // 4 values are always stored, even if alpha does not exists 
                else
                    for (size_t i = 0; i < (alpha_plane ? 4 : 3); i++)
                        Planes.push_back(new plane(width, height, (bits_per_raw_sample + 7) / 8)); // R, G, B, optionnaly A
                break;
        default: ; 
    }
}

//---------------------------------------------------------------------------
void raw_frame::DPX_Create(size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample)
{
    switch (colorspace_type)
    {
        case 1: // JPEG2000-RCT --> RGB
                Planes.push_back(new plane(width, height, dpx::BitsPerPixel((dpx::style)Style_Private)));
        default: ;
    }
}
