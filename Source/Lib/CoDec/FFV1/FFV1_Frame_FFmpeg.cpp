/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Frame_FFmpeg.h"
#ifdef RAWCOOKED_FFMPEG
#include "Lib/CoDec/FFmpeg_Functions.h"
#include "ThreadPool.h"
#include "Lib/Transform/Transform.h"
#include "Lib/Utils/RawFrame/RawFrame.h"
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Decoded frames, from Decode() to Output() which may be in different threads
struct ffv1_frame_ffmpeg_frames
{
    std::mutex                  Mutex;
    std::deque<AVFrame*>        Decoded; // In decoding order, empty frame if decoding failed
    std::vector<AVFrame*>       Free; // For reuse

    ~ffv1_frame_ffmpeg_frames()
    {
        for (auto Frame : Decoded)
            ffmpeg::av_frame_free(&Frame);
        for (auto Frame : Free)
            ffmpeg::av_frame_free(&Frame);
    }
};

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
static AVPixelFormat Vulkan_Format(AVCodecContext*, const AVPixelFormat* Formats)
{
    // Vulkan only, so decoding fails if Vulkan decoding is not possible
    while (*Formats != AV_PIX_FMT_NONE && *Formats != AV_PIX_FMT_VULKAN)
        Formats++;
    return *Formats;
}

//---------------------------------------------------------------------------
ffv1_frame_ffmpeg::ffv1_frame_ffmpeg(ThreadPool* Pool_, bool Vulkan) :
    RawFrame(nullptr),
    Pool(Pool_),
    Context(ffmpeg::avcodec_alloc_context3(ffmpeg::avcodec_find_decoder(AV_CODEC_ID_FFV1))),
    Packet(ffmpeg::av_packet_alloc()),
    Frames(new ffv1_frame_ffmpeg_frames),
    Frame_Sw(ffmpeg::av_frame_alloc())
{
    Context->thread_type = FF_THREAD_SLICE; // Frame is output immediately (frame threading is done by Decode()/Output())
    Context->thread_count = Pool ? 0 : 1;
    if (Vulkan)
    {
        ffmpeg::av_hwdevice_ctx_create(&Context->hw_device_ctx, AV_HWDEVICE_TYPE_VULKAN, nullptr, nullptr, 0);
        Context->get_format = Vulkan_Format;
    }
}

//---------------------------------------------------------------------------
ffv1_frame_ffmpeg::~ffv1_frame_ffmpeg()
{
    ffmpeg::av_frame_free(&Frame_Sw);
    delete Frames;
    ffmpeg::av_packet_free(&Packet);
    ffmpeg::avcodec_free_context(&Context);
}

//***************************************************************************
// External metadata
//***************************************************************************

//---------------------------------------------------------------------------
void ffv1_frame_ffmpeg::SetWidth(uint32_t width)
{
    Context->width = (int)width;
}

//---------------------------------------------------------------------------
void ffv1_frame_ffmpeg::SetHeight(uint32_t height)
{
    Context->height = (int)height;
}

//***************************************************************************
// Actions
//***************************************************************************

//---------------------------------------------------------------------------
bool ffv1_frame_ffmpeg::OutOfBand(const uint8_t* Buffer, size_t Buffer_Size)
{
    ffmpeg::av_freep(&Context->extradata);
    Context->extradata = (uint8_t*)ffmpeg::av_mallocz(Buffer_Size + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(Context->extradata, Buffer, Buffer_Size);
    Context->extradata_size = (int)Buffer_Size;
    return false;
}

//---------------------------------------------------------------------------
size_t ffv1_frame_ffmpeg::Padding()
{
    return AV_INPUT_BUFFER_PADDING_SIZE;
}

//---------------------------------------------------------------------------
// Count of parts for our parallel work
static size_t Parallel_Parts()
{
    static const size_t Count = std::thread::hardware_concurrency() ? std::thread::hardware_concurrency() : 4;
    return Count;
}

//---------------------------------------------------------------------------
// Run Function(Begin, End) on Count parts of [0, Size) in parallel, parts aligned on Align
template<typename F>
static void Parallel(ThreadPool* Pool, size_t Count, size_t Size, size_t Align, F&& Function)
{
    auto Part = ((Size + Count - 1) / Count + Align - 1) / Align * Align;
    std::vector<std::future<int>> Futures;
    for (size_t Begin = 0; Begin < Size; Begin += Part)
    {
        auto End = Begin + Part < Size ? Begin + Part : Size;
        if (Pool)
            Futures.push_back(Pool->submit(Function, Begin, End));
        else
            Function(Begin, End);
    }
    for (auto& Future : Futures)
        Future.get();
}

//---------------------------------------------------------------------------
bool ffv1_frame_ffmpeg::Decode(AVBufferRef* Buffer, size_t Buffer_Size)
{
    // Frame for the decoding, given to Output() even if decoding fails (empty frame)
    AVFrame* Frame;
    {
        std::lock_guard<std::mutex> Lock(Frames->Mutex);
        if (Frames->Free.empty())
            Frame = ffmpeg::av_frame_alloc();
        else
        {
            Frame = Frames->Free.back();
            Frames->Free.pop_back();
        }
    }
    auto Result = Decode(Frame, Buffer, Buffer_Size);
    std::lock_guard<std::mutex> Lock(Frames->Mutex);
    Frames->Decoded.push_back(Frame);
    return Result;
}

//---------------------------------------------------------------------------
bool ffv1_frame_ffmpeg::Decode(AVFrame* Frame, AVBufferRef* Buffer, size_t Buffer_Size)
{
    if (!ffmpeg::avcodec_is_open(Context) && ((Context->get_format == Vulkan_Format && !Context->hw_device_ctx) || ffmpeg::avcodec_open2(Context, nullptr, nullptr) < 0))
        return true; // Vulkan device is not available, or other error
    Packet->buf = ffmpeg::av_buffer_ref(Buffer);
    if (!Packet->buf)
        return true;
    Packet->data = Buffer->data;
    Packet->size = (int)Buffer_Size;
    auto Result = ffmpeg::avcodec_send_packet(Context, Packet); // Decoding is synchronous (slice threads only), FFmpeg keeps its own reference to the buffer if it needs it after the decoding (e.g. Vulkan)
    ffmpeg::av_packet_unref(Packet);
    return Result < 0 || ffmpeg::avcodec_receive_frame(Context, Frame) < 0;
}

//---------------------------------------------------------------------------
bool ffv1_frame_ffmpeg::Output()
{
    // Frame in the same order as Decode()
    AVFrame* Frame;
    {
        std::lock_guard<std::mutex> Lock(Frames->Mutex);
        if (Frames->Decoded.empty())
            return true;
        Frame = Frames->Decoded.front();
        Frames->Decoded.pop_front();
    }
    auto Result = Output(Frame);
    ffmpeg::av_frame_unref(Frame);
    std::lock_guard<std::mutex> Lock(Frames->Mutex);
    Frames->Free.push_back(Frame);
    return Result;
}

//---------------------------------------------------------------------------
void ffv1_frame_ffmpeg::Discard()
{
    std::lock_guard<std::mutex> Lock(Frames->Mutex);
    if (Frames->Decoded.empty())
        return;
    auto Frame = Frames->Decoded.back();
    Frames->Decoded.pop_back();
    ffmpeg::av_frame_unref(Frame);
    Frames->Free.push_back(Frame);
}

//---------------------------------------------------------------------------
bool ffv1_frame_ffmpeg::Output(AVFrame* Frame)
{
    if (!Frame->buf[0])
        return true; // Decoding failed
    size_t Parts = Pool ? Parallel_Parts() : 1;

    // Download from GPU memory if hardware decoding (waiting for the end of the decoding), then release the GPU frame
    // Buffers of the downloaded frame are reused (no new memory pages for each frame)
    auto Picture = Frame;
    if (Frame->hw_frames_ctx)
    {
        if (Frame_Sw->buf[0] && (Frame_Sw->width != Frame->width || Frame_Sw->height != Frame->height || Frame_Sw->format != ((AVHWFramesContext*)Frame->hw_frames_ctx->data)->sw_format))
            ffmpeg::av_frame_unref(Frame_Sw);
        auto Result = ffmpeg::av_hwframe_transfer_data(Frame_Sw, Frame, 0);
        ffmpeg::av_frame_unref(Frame);
        if (Result < 0)
        {
            ffmpeg::av_frame_unref(Frame_Sw);
            return true;
        }
        Picture = Frame_Sw;
    }

    // Raw frame configuration, from the decoded pixel format
    auto Desc = ffmpeg::av_pix_fmt_desc_get((AVPixelFormat)Picture->format);
    size_t Bits = Desc->comp[0].depth;
    size_t IsRGB = (Desc->flags & AV_PIX_FMT_FLAG_RGB) ? 1 : 0;
    size_t HasAlpha = (Desc->flags & AV_PIX_FMT_FLAG_ALPHA) ? 1 : 0;
    size_t w = Picture->width;
    size_t h = Picture->height;
    RawFrame->Create(w, h, 0
        | ((Bits - 1)                           <<  0)
        | ((Desc->nb_components > 2 ? 1 : 0)    <<  6)
        | (HasAlpha                             <<  7)
        | ((1 << Desc->log2_chroma_w)           <<  8)
        | ((1 << Desc->log2_chroma_h)           << 12)
        | (IsRGB                                << 16)
        ); // Transfered as 1 horizontal slice

    // Direct transfer of planar RGB without JPEG2000-RCT if the output format is supported, by bands of lines in parallel
    if (IsRGB && Desc->flags & AV_PIX_FMT_FLAG_PLANAR && (Desc->flags & AV_PIX_FMT_FLAG_BE) == AV_NE(AV_PIX_FMT_FLAG_BE, 0) && Desc->nb_components == 3 + HasAlpha)
    {
        auto Direct = Transform_Direct_Init(RawFrame, Bits, HasAlpha);
        for (int c = 0; Direct && c < Desc->nb_components; c++)
        {
            const auto& Comp = Desc->comp[c];
            if ((size_t)Comp.depth != Bits || Comp.shift || Comp.offset || Comp.step != (Bits > 8 ? 2 : 1))
                Direct = nullptr;
        }
        if (Direct)
        {
            auto Plane = RawFrame->Plane(0);
            Parallel(Pool, Parts < 4 ? Parts : 4, h, 1, [&](size_t y_Begin, size_t y_End) -> int // Limited by memory bandwidth, more threads would only wait
            {
                for (size_t y = y_Begin; y < y_End; y++)
                {
                    const uint8_t* Src[4];
                    for (int c = 0; c < Desc->nb_components; c++)
                        Src[c] = Picture->data[Desc->comp[c].plane] + y * Picture->linesize[Desc->comp[c].plane];
                    Direct(Plane->Buffer_Data(0, y), Src, w);
                }
                return 0;
            });
            return false;
        }
    }

    // Transfer of a band of lines, with the same transforms as our decoder
    auto Transfer = [&](size_t y_Begin, size_t y_End) -> int
    {
        // Lines with some room after the end of the line, like in our decoder
        std::vector<pixel_t> Lines(4 * (w + 3));
        pixel_t* Line[4];
        for (size_t i = 0; i < 4; i++)
            Line[i] = Lines.data() + i * (w + 3);
        auto Read = [&](int c, size_t y, size_t Width, pixel_t* Dest)
        {
            // Direct read of 8-bit or native endian 16-bit storage (2x faster), else generic FFmpeg read
            const auto& Comp = Desc->comp[c];
            const auto Src = Picture->data[Comp.plane] + y * Picture->linesize[Comp.plane] + Comp.offset;
            const size_t Step = Comp.step;
            if (!Comp.shift && Comp.depth == 8)
            {
                for (size_t x = 0; x < Width; x++)
                    Dest[x] = Src[x * Step];
            }
            else if (!Comp.shift && Comp.depth <= 16 && !(Step & 1) && (Desc->flags & AV_PIX_FMT_FLAG_BE) == AV_NE(AV_PIX_FMT_FLAG_BE, 0))
            {
                const auto Src16 = (const uint16_t*)Src;
                const auto Step16 = Step / 2;
                const auto Mask = (pixel_t)((1 << Comp.depth) - 1);
                for (size_t x = 0; x < Width; x++)
                    Dest[x] = Src16[x * Step16] & Mask;
            }
            else
                ffmpeg::av_read_image_line2(Dest, (const uint8_t**)Picture->data, Picture->linesize, Desc, 0, (int)y, c, (int)Width, 0, (int)sizeof(pixel_t));
        };
        auto Transform = Transform_Init(RawFrame, IsRGB ? pix_style::RGBA : pix_style::YUVA, Bits, 0, y_Begin, w, y_End - y_Begin);
        if (IsRGB)
        {
            // Transforms expect content as coded (JPEG2000-RCT, with g and b inverted if 9-15 bits without alpha)
            auto Offset = ((pixel_t)1) << Bits;
            auto Inverted = Bits > 8 && Bits < 16 && !HasAlpha;
            for (size_t y = y_Begin; y < y_End; y++)
            {
                Read(0, y, w, Line[2]);
                Read(1, y, w, Line[Inverted ? 1 : 0]);
                Read(2, y, w, Line[Inverted ? 0 : 1]);
                if (HasAlpha)
                    Read(3, y, w, Line[3]);
                for (size_t x = 0; x < w; x++)
                {
                    pixel_t g = Line[0][x];
                    pixel_t b = Line[1][x] - g;
                    pixel_t r = Line[2][x] - g;
                    Line[0][x] = g + ((b + r) >> 2);
                    Line[1][x] = b + Offset;
                    Line[2][x] = r + Offset;
                }
                Transform->From(Line[0], Line[1], Line[2], Line[3]);
            }
        }
        else
        {
            // Plane then line: Y, Cb, Cr, A
            for (int c = 0; c < Desc->nb_components; c++)
            {
                auto IsChroma = c == 1 || c == 2; // No chroma subsampling if luma + alpha only
                auto Shift_h = IsChroma ? Desc->log2_chroma_h : 0;
                auto Plane_w = (size_t)AV_CEIL_RSHIFT((int)w, IsChroma ? Desc->log2_chroma_w : 0);
                auto Plane_y_End = (size_t)AV_CEIL_RSHIFT((int)y_End, Shift_h);
                for (size_t y = y_Begin >> Shift_h; y < Plane_y_End; y++)
                {
                    Read(c, y, Plane_w, Line[0]);
                    Transform->From(Line[0]);
                }
            }
        }
        delete Transform;
        return 0;
    };

    // Bands of lines in parallel, chroma lines not split between bands
    Parallel(Pool, Parts, h, (size_t)1 << Desc->log2_chroma_h, Transfer);
    RawFrame->Finalize(1, 1);

    return false;
}

#endif //RAWCOOKED_FFMPEG
