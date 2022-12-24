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
#include "Lib/Common/Common.h"
#include <cstring>
#include <vector>
using namespace std;
//---------------------------------------------------------------------------

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
        plane(size_t NewWidth, size_t NewHeight, size_t NewBitsPerBlock, size_t NewPixelsPerBlock = 1, size_t Line_BitsBefore = 0, size_t Line_BitsAfter = 0, size_t Line_Alignment = 0, size_t Line_SliceCount = 0)
            :
            Line_BitsBefore_(Line_BitsBefore),
            Line_BitsAfter_(Line_BitsAfter),
            Line_Alignment_(Line_Alignment),
            Line_SliceCount_(Line_SliceCount),
            Width_(NewWidth),
            Height_(NewHeight),
            BitsPerBlock_(NewBitsPerBlock),
            PixelsPerBlock_(NewPixelsPerBlock)
        {
            size_t BitSize;
            if ((intptr_t)Line_BitsAfter_ < 0)
            {
                auto PixelCount = Width_ * Height_;
                auto BlockCount = PixelCount / PixelsPerBlock_;
                auto PixelRemain = PixelCount % PixelsPerBlock_;
                if (PixelRemain)
                    BlockCount++;
                BitSize = BlockCount * NewBitsPerBlock;
            }
            else
                BitSize = BitsPerLine() * Height_;
            if (Line_Alignment_)
            {
                auto UnalignedBits = BitSize % Line_Alignment_;
                if (UnalignedBits)
                    BitSize += Line_Alignment_ - UnalignedBits;
            }
            auto ByteSize = BitSize >> 3;
            if (BitSize & 0x7)
                ByteSize++;
            Buffer_.Create(ByteSize);
            if (Line_SliceCount_)
                Buffer_Extra_.Create(Height_ * Line_SliceCount_ * (Line_Alignment / 8));
        }

        const buffer& Buffer() const
        {
            return Buffer_;
        }

        const buffer& Buffer_Extra() const
        {
            return Buffer_Extra_;
        }

        uint8_t* Buffer_Data(size_t x = 0, size_t y = 0, bool LastBlock = false) const
        {
            if (!x && !y)
                return Buffer_.Data();
            if ((intptr_t)Line_BitsAfter_ < 0)
            {
                size_t PixelCountBefore = y * Width_ + x;
                size_t BlockCountBefore = PixelCountBefore / PixelsPerBlock_;
                return Buffer().Data() + BlockCountBefore * BitsPerBlock() / 8;
            }
            else
            {
                auto Value =  y * BitsPerLine() + x / PixelsPerBlock_ * BitsPerBlock_;
                auto WidthInIncompleteBlock = x % PixelsPerBlock_;
                if (WidthInIncompleteBlock)
                {
                    if (!LastBlock)
                        WidthInIncompleteBlock--;
                    WidthInIncompleteBlock *= BitsPerBlock_ / PixelsPerBlock_;
                    Value += WidthInIncompleteBlock;
                    auto UnalignedBits = Value % Line_Alignment_;
                    if (UnalignedBits)
                        Value -= UnalignedBits;
                    else if (LastBlock)
                        Value -= Line_Alignment_;
                }
                return Buffer_.Data() + Value / 8;
            }
        }

        size_t ValidBitsPerLine() const
        {
            return Width_ * BitsPerBlock_ / PixelsPerBlock_;
        }

        size_t BitsPerLine() const
        {
            auto Value = Line_BitsBefore_ + Width_ / PixelsPerBlock_ * BitsPerBlock_;
            auto WidthInIncompleteBlock = Width_ % PixelsPerBlock_;
            if (WidthInIncompleteBlock)
            {
                WidthInIncompleteBlock *= BitsPerBlock_ / PixelsPerBlock_;
                Value += WidthInIncompleteBlock;
                auto UnalignedBits = Value % Line_Alignment_;
                if (UnalignedBits)
                    Value += Line_Alignment_ - UnalignedBits;
            }
            return Value + Line_BitsAfter_;
        }

        size_t Line_BitsAfter() const
        {
            return Line_BitsAfter_;
        }

        size_t Line_SliceCount() const
        {
            return Line_SliceCount_;
        }

        size_t Width() const
        {
            return Width_;
        }

        size_t Height() const
        {
            return Height_;
        }

        size_t BitsPerBlock() const
        {
            return BitsPerBlock_;
        }

        size_t PixelsPerBlock() const
        {
            return PixelsPerBlock_;
        }

    private:
        buffer                  Buffer_;
        buffer                  Buffer_Extra_;
        size_t                  Line_BitsBefore_;
        size_t                  Line_BitsAfter_;
        size_t                  Line_Alignment_;
        size_t                  Line_SliceCount_;
        size_t                  Width_;
        size_t                  Height_;
        size_t                  BitsPerBlock_;
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

    // Creation (info: 0-5: bits_per_raw_sample-1, 6: chroma_planes, 7: alpha_plane, 8-11: h_chroma_subsample, 12-15: v_chroma_subsample, 16-23: colorspace_type, 24-31: line_slice_count-1)
    void Create(size_t width, size_t height, size_t info);

    // Info
    size_t FrameSize() const;
    size_t TotalSize() const;

    // Processing
    void Process();
    raw_frame_process* FrameProcess = nullptr;
    void Finalize(size_t num_h_slices, size_t num_v_slices) { if (Finalize_Function) Finalize_Function(this, num_h_slices, num_v_slices); }
    void (*Finalize_Function)(raw_frame*, size_t, size_t) = nullptr;

//private:
    buffer_or_view              Buffer_;
    std::vector<plane*>         Planes_;
    buffer_or_view              Pre_;
    buffer_or_view              Post_;
    buffer_or_view              In_;
    void FFmpeg_Create(size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample);
    void DPX_Create(size_t colorspace_type, size_t width, size_t height, size_t line_slice_count);
    void TIFF_Create(size_t colorspace_type, size_t width, size_t height);
    void EXR_Create(size_t colorspace_type, size_t width, size_t height);
    void MergeIn();
};

//---------------------------------------------------------------------------
#endif
