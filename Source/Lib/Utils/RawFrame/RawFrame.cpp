/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include "Lib/Uncompressed/EXR/EXR.h"
#include "Lib/Uncompressed/AVI/AVI.h"
#include <algorithm>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void raw_frame::Create(size_t width, size_t height, size_t info)
{
    if (!Planes_.empty())
        return; //TODO: manage when it changes

    for (const auto& Plane : Planes_)
        delete Plane;
    Planes_.clear();

    auto colorspace_type = (info >> 16) & 0xFF;
    switch (Flavor)
    {
        case flavor::FFmpeg:
        {
            auto bits_per_raw_sample    = ((info >>  0) & 0x3F) + 1;
            auto chroma_planes          =  (info >>  6) & 0x01;
            auto alpha_plane            =  (info >>  7) & 0x01;
            auto h_chroma_subsample     =  (info >>  8) & 0x0F;
            auto v_chroma_subsample     =  (info >> 12) & 0x0F;
            FFmpeg_Create(colorspace_type, width, height, bits_per_raw_sample, chroma_planes, alpha_plane, h_chroma_subsample, v_chroma_subsample);
            break;
        }
        case flavor::DPX:
        {
            auto line_slice_count       = ((info >> 24) & 0xFF) + 1;
            DPX_Create(colorspace_type, width, height, line_slice_count);
            break;
        }
        case flavor::TIFF:
        {
            TIFF_Create(colorspace_type, width, height); 
            break;
        }
        case flavor::EXR:
        {
            EXR_Create(colorspace_type, width, height); 
            break;
        }
        case flavor::AVI: AVI_Create(colorspace_type, width, height); break;
        case flavor::None:;
    }
}

//---------------------------------------------------------------------------
void raw_frame::FFmpeg_Create(size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample)
{
    switch (colorspace_type)
    {
        case 0: // YCbCr --> YA Packed if no chroma, alpha and <=8, else YUV Planar
                Planes_.push_back(new plane(width, height, (bits_per_raw_sample + 7) / 8 * ((bits_per_raw_sample <= 8 && !chroma_planes && alpha_plane) ? 2 : 1))); // Luma
                if (chroma_planes)
                {
                    size_t h_divisor = h_chroma_subsample;
                    size_t v_divisor = v_chroma_subsample;
                    for (size_t i = 0; i < 2; i++)
                        Planes_.push_back(new plane((width + h_divisor - 1) / h_divisor, (height + v_divisor - 1) / v_divisor, bits_per_raw_sample / 8 + ((bits_per_raw_sample % 8) ? 1 : 0))); //Chroma
                }
                if (alpha_plane && !(bits_per_raw_sample <= 8 && !chroma_planes))
                    Planes_.push_back(new plane(width, height, (bits_per_raw_sample + 7) / 8)); //Alpha
                break;
        case 1: // JPEG2000-RCT --> RGB Packed if <=8, else RGB planar
                if (bits_per_raw_sample <= 8)
                    Planes_.push_back(new plane(width, height, (bits_per_raw_sample + 7) / 8 * 4)); // 4 values are always stored, even if alpha does not exists 
                else
                    for (size_t i = 0; i < (alpha_plane ? 4 : 3); i++)
                        Planes_.push_back(new plane(width, height, (bits_per_raw_sample + 7) / 8)); // R, G, B, optionnaly A
                break;
        default: ; 
    }
}

//---------------------------------------------------------------------------
void raw_frame::DPX_Create(size_t colorspace_type, size_t width, size_t height, size_t line_slice_count)
{
    switch (colorspace_type)
    {
        case 0: // YUV
        case 1: // JPEG2000-RCT --> RGB
        {
            if (dpx::PixelsPerBlock((dpx::flavor)Flavor_Private) == 1)
                line_slice_count = 0;
            size_t WidthPadding;
            if (dpx::IsAltern(Flavor_Private))
            {
                size_t width_Remaining = width % 3;
                if (width_Remaining)
                    WidthPadding = width_Remaining * 10 - 32;
                else
                    WidthPadding = 0;
            }
            else
                WidthPadding = 0;
            Planes_.push_back(new plane(width, height, dpx::BytesPerBlock((dpx::flavor)Flavor_Private) * 8, dpx::PixelsPerBlock((dpx::flavor)Flavor_Private), 0, WidthPadding, 32, line_slice_count));
        }
        default:;
    }
}

//---------------------------------------------------------------------------
void raw_frame::TIFF_Create(size_t colorspace_type, size_t width, size_t height)
{
    switch (colorspace_type)
    {
        case 0: // YUV
        case 1: // JPEG2000-RCT --> RGB
                Planes_.push_back(new plane(width, height, tiff::BytesPerBlock((tiff::flavor)Flavor_Private) * 8, tiff::PixelsPerBlock((tiff::flavor)Flavor_Private)));
        default: ;
    }
}

//---------------------------------------------------------------------------
void raw_frame::EXR_Create(size_t colorspace_type, size_t width, size_t height)
{
    switch (colorspace_type)
    {
        case 0: // YUV
        case 1: // JPEG2000-RCT --> RGB
                Planes_.push_back(new plane(width, height, exr::BytesPerBlock((exr::flavor)Flavor_Private) * 8, exr::PixelsPerBlock((exr::flavor)Flavor_Private), 0, 8 * 8));
        default: ;
    }
}

//---------------------------------------------------------------------------
void raw_frame::AVI_Create(size_t colorspace_type, size_t width, size_t height)
{
    switch (colorspace_type)
    {
    case 0: // YUV
    case 1: // JPEG2000-RCT --> RGB
        Planes_.push_back(new plane(width, height, avi::BytesPerBlock((avi::flavor)Flavor_Private) * 8, avi::PixelsPerBlock((avi::flavor)Flavor_Private)));
    default:;
    }
}

//---------------------------------------------------------------------------
size_t raw_frame::FrameSize() const
{
    auto FrameSize = Buffer_.Size();

    for (const auto& Plane : Planes_)
        FrameSize += Plane->Buffer().Size();

    return FrameSize;
}

//---------------------------------------------------------------------------
size_t raw_frame::TotalSize() const
{
    return Pre_.Size() + max(In_.Size(), FrameSize()) + Post_.Size();
}

//---------------------------------------------------------------------------
void raw_frame::Process()
{
    MergeIn();
    In_.Clear();

    if (FrameProcess)
        FrameProcess->FrameCall(this);

    Pre_.Clear();
    Post_.Clear();
}

//---------------------------------------------------------------------------
void raw_frame::MergeIn()
{
    auto Buffer_Size = Buffer_.Size();
    if (Buffer_Size && Buffer_Size == In_.Size())
    {
        auto Buffer_Data = Buffer_.DataForModification();
        auto In_Data = In_.Data();
        for (size_t i = 0; i < Buffer_Size; i++)
            Buffer_Data[i] ^= In_Data[i];
    }

    if (Planes_.size() == 1 && Planes_[0])
    {
        auto Buffer_Size = Planes_[0]->Buffer().Size();
        if (Buffer_Size && Buffer_Size == In_.Size())
        {
            auto Buffer_Data = Planes_[0]->Buffer().Data();
            auto In_Data = In_.Data();
            for (size_t i = 0; i < Buffer_Size; i++)
                Buffer_Data[i] ^= In_Data[i];
        }
    }
}
