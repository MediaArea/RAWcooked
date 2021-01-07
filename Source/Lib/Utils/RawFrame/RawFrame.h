/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef RawFrameH
#define RawFrameH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Utils/Buffer/Buffer.h"
#include "Lib/Config.h"
#include <cstring>
#include <vector>
using namespace std;
//---------------------------------------------------------------------------

class raw_frame;
class raw_frame_process
{
private:
    virtual void                FrameCall(raw_frame* RawFrame) = 0;
    friend class raw_frame;
};

class raw_frame
{
public:
    uint64_t                    Flavor_Private = 0; //Used by specialized flavor for marking the configuration of such flavor (e.g. endianness of DPX)

    struct plane
    {
        plane(size_t NewWidth, size_t NewHeight, size_t NewBytesPerBlock, size_t NewPixelsPerBlock = 1, size_t Width_Prefix = 0)
            :
            Width_(NewWidth),
            Height_(NewHeight),
            BytesPerBlock_(NewBytesPerBlock),
            PixelsPerBlock_(NewPixelsPerBlock),
            Width_Prefix_(Width_Prefix)
        {
            Width_Padding_ = 0; //TODO: option for padding size
            if (Width_Padding_)
                Width_Padding_ -= Width_ % Width_Padding_;

            Buffer_.Create(AllBytesPerLine() * Height_);
        }

        const buffer& Buffer() const
        {
            return Buffer_;
        }

        size_t ValidBytesPerLine() const
        {
            return Width_ * BytesPerBlock_ / PixelsPerBlock_;
        }

        size_t AllBytesPerLine() const
        {
            return Width_Prefix_ + (Width_ * BytesPerBlock_ / PixelsPerBlock_) + Width_Padding_;
        }

        size_t BytesPerBlock() const
        {
            return BytesPerBlock_;
        }

        size_t PixelsPerBlock() const
        {
            return PixelsPerBlock_;
        }

    //private:
        buffer                  Buffer_;
        size_t                  Width_;
        size_t                  Width_Prefix_;
        size_t                  Width_Padding_;
        size_t                  Height_;
        size_t                  BytesPerBlock_;
        size_t                  PixelsPerBlock_;
    };

    const buffer_or_view& Pre() const
    {
        return Pre_;
    }

    void SetPre(buffer_or_view&& NewPre)
    {
        Pre_ = move(NewPre);
    }

    const buffer_or_view& Post() const
    {
        return Post_;
    }

    void SetPost(buffer_or_view&& NewPost)
    {
        Post_ = move(NewPost);
    }

    void SetIn(buffer_or_view&& NewIn)
    {
        In_ = move(NewIn);
    }

    buffer_or_view& Buffer()
    {
        return Buffer_;
    }

    const buffer_or_view& Buffer() const
    {
        return Buffer_;
    }
    
    void AssignBufferView(const uint8_t* NewData, size_t NewSize)
    {
        Buffer_ = buffer_or_view(NewData, NewSize);
    }
    
    const std::vector<plane*> Planes() const
    {
        return Planes_;
    }

    const plane* Plane(size_t p) const
    {
        return Planes_[p];
    }

    ENUM_BEGIN(flavor)
        None,
        FFmpeg,
        DPX,
        TIFF,
        EXR,
    ENUM_END(flavor)
    flavor                       Flavor = flavor::None;

    ~raw_frame()
    {
        for (const auto& Plane : Planes_)
            delete Plane;
    }

    // Creation
    void Create(size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample);

    // Info
    size_t FrameSize() const;
    size_t TotalSize() const;

    // Processing
    void Process();
    raw_frame_process* FrameProcess = nullptr;

//private:
    buffer_or_view              Buffer_;
    std::vector<plane*>         Planes_;
    buffer_or_view              Pre_;
    buffer_or_view              Post_;
    buffer_or_view              In_;
    void FFmpeg_Create(size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample);
    void DPX_Create(size_t colorspace_type, size_t width, size_t height);
    void TIFF_Create(size_t colorspace_type, size_t width, size_t height);
    void EXR_Create(size_t colorspace_type, size_t width, size_t height);
    void MergeIn();
};

//---------------------------------------------------------------------------
#endif
