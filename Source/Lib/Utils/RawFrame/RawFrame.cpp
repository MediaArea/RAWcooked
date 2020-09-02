/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include <algorithm>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void raw_frame::Create(size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample)
{
    if (!Planes_.empty())
        return; //TODO: manage when it changes

    for (const auto& Plane : Planes_)
        delete Plane;
    Planes_.clear();

    switch (Flavor)
    {
        case flavor::FFmpeg: FFmpeg_Create(colorspace_type, width, height, bits_per_raw_sample, chroma_planes, alpha_plane, h_chroma_subsample, v_chroma_subsample); break;
        case flavor::DPX: DPX_Create(colorspace_type, width, height); break;
        case flavor::TIFF: TIFF_Create(colorspace_type, width, height); break;
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
void raw_frame::DPX_Create(size_t colorspace_type, size_t width, size_t height)
{
    switch (colorspace_type)
    {
        case 1: // JPEG2000-RCT --> RGB
                Planes_.push_back(new plane(width, height, dpx::BytesPerBlock((dpx::flavor)Flavor_Private), dpx::PixelsPerBlock((dpx::flavor)Flavor_Private)));
        default: ;
    }
}

//---------------------------------------------------------------------------
void raw_frame::TIFF_Create(size_t colorspace_type, size_t width, size_t height)
{
    switch (colorspace_type)
    {
        case 1: // JPEG2000-RCT --> RGB
                Planes_.push_back(new plane(width, height, tiff::BytesPerBlock((tiff::flavor)Flavor_Private), tiff::PixelsPerBlock((tiff::flavor)Flavor_Private)));
        default: ;
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
