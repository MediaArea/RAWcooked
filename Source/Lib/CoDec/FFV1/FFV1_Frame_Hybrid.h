/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFV1_Frame_HybridH
#define FFV1_Frame_HybridH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstddef>
#include <cstdint>
struct ffv1_frame_hybrid_private;
//---------------------------------------------------------------------------

//***************************************************************************
// Class frame, decoded in other threads by FFmpeg decoder (CPU) and/or FFmpeg decoder with Vulkan (GPU), and optionally our decoder (CPU), in parallel (available if RAWCOOKED_FFMPEG is defined)
//***************************************************************************

// Each frame is sent to the first available decoder (to the decoder of the previous frame if it is not a keyframe),
// frames decoded by an FFmpeg decoder are transferred to raw frames in another thread, during the decoding of next frames.

class ThreadPool;
class data_holder;
class raw_frame;
class raw_frame_ready;

class ffv1_frame_hybrid
{
public:
    // Constructor/Destructor
    ffv1_frame_hybrid(ThreadPool* Pool, bool UseOurDecoder, bool UseFFmpegDecoder, bool UseVulkanDecoder); // At least one FFmpeg decoder (UseVulkanDecoder: FFmpeg decoder with Vulkan hardware decoding)
    ~ffv1_frame_hybrid();

    // External metadata
    void SetWidth(uint32_t width);
    void SetHeight(uint32_t height);

    // Actions
    bool OutOfBand(const uint8_t* Buffer, size_t Buffer_Size);
    static size_t Padding(); // Count of bytes FFmpeg reads after the frame data

    // Decoding to Frame in other threads, FrameReady->Ready(Opaque, Error) is called when the frame is decoded (frames may be ready not in order)
    // The frame data is used after the call through its holder (no copy)
    void Decode(const uint8_t* Buffer, size_t Buffer_Size, data_holder* Holder, raw_frame* Frame, void* Opaque);
    size_t Depth(); // Max count of frames between Decode() and the end of their use by the caller
    void End(); // No more frames, decoders are released after the decoding of pending frames (during the output of last frames)

    // Configuration of decoded frames (flavor)
    raw_frame*                  RawFrame;
    raw_frame_ready*            FrameReady;
    size_t                      Remaining; // Count of frames after the frame given to Decode(), (size_t)-1 if unknown

private:
    ffv1_frame_hybrid_private*  P;
};

//---------------------------------------------------------------------------
#endif
