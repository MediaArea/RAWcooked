/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFV1_Frame_FFmpegH
#define FFV1_Frame_FFmpegH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstddef>
#include <cstdint>
struct AVCodecContext;
struct AVPacket;
struct AVFrame;
struct AVBufferRef;
struct ffv1_frame_ffmpeg_frames;
//---------------------------------------------------------------------------

//***************************************************************************
// Class frame, decoded by FFmpeg libavcodec (available if RAWCOOKED_FFMPEG is defined)
//***************************************************************************

class ThreadPool;
class raw_frame;

class ffv1_frame_ffmpeg
{
public:
    // Constructor/Destructor
    ffv1_frame_ffmpeg(ThreadPool* Pool, bool Vulkan); // Vulkan: hardware decoding with Vulkan
    ~ffv1_frame_ffmpeg();

    // External metadata
    void SetWidth(uint32_t width);
    void SetHeight(uint32_t height);

    // Actions
    bool OutOfBand(const uint8_t* Buffer, size_t Buffer_Size);
    static size_t Padding(); // Count of bytes FFmpeg reads after the frame data

    // Process in 2 steps: Decode() of next frames may run during Output() of current frame (from another thread)
    bool Decode(AVBufferRef* Buffer, size_t Buffer_Size); // Buffer with FFmpeg padding, a reference is kept by FFmpeg if needed (no copy)
    size_t RawFrame_Index = (size_t)-1; // Index of the frame given to Decode(), for instrumentation
    bool Output(); // To raw frame
    void Discard(); // Last decoded frame, instead of Output()

    // Decoded frame
    raw_frame*                  RawFrame;

private:
    bool Decode(AVFrame* Frame, AVBufferRef* Buffer, size_t Buffer_Size);
    bool Output(AVFrame* Frame);

    ThreadPool*                 Pool;

    // FFmpeg
    AVCodecContext*             Context;
    AVPacket*                   Packet;
    ffv1_frame_ffmpeg_frames*   Frames; // Decoded frames, from Decode() to Output()
    AVFrame*                    Frame_Sw; // Downloaded from GPU memory, buffers are reused
};

//---------------------------------------------------------------------------
#endif
