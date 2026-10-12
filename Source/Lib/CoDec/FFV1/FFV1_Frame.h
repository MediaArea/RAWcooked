/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFV1_FrameH
#define FFV1_FrameH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Slice.h"
#include <condition_variable>
#include <functional>
#include <mutex>
//---------------------------------------------------------------------------

//***************************************************************************
// Class frame
//***************************************************************************

class ThreadPool;
class raw_frame;
struct ffv1_frame_slot;

class ffv1_frame
{
public:
    // Constructor/Destructor
    ffv1_frame(ThreadPool* Pool);
    ~ffv1_frame();

    // External metadata
    void SetWidth(uint32_t width);
    void SetHeight(uint32_t width);

    // Actions
    // Decoding of Buffer to RawFrame, Done(Error) is called when the frame is decoded (in a thread of the pool if there is a pool), Buffer must stay valid until this call
    // With a pool, returns when all slices of the frame are being decoded, so the slices of the next frame are decoded during the decoding of the last slices of this frame
    void Process(const uint8_t* Buffer, size_t Buffer_Size, std::function<void(bool Error)> Done);
    void Wait(); // Waits for the end of the decoding of all frames
    bool OutOfBand(const uint8_t* Buffer, size_t Buffer_Size);

    // Decoded frame
    raw_frame*                  RawFrame;

    // Error message
    const char* ErrorMessage() const;

    // Info
    bool Intra() const { return P.ConfigurationRecord_IsPresent && P.intra; } // All frames are keyframes

private:
    // Parameters
    parameters                  P;
    rangecoder                  E;

    // Temp
    bool                        KeyFrame_IsPresent;
    ThreadPool*                 Pool;

    // Frames being decoded, each one with its slices: a keyframe uses a free slot, other frames use the slot of the previous frame (decoding states of the previous frame)
    ffv1_frame_slot*            Slots;
    size_t                      Slot_Last = 0;
    std::mutex                  Mutex;
    std::condition_variable     Condition;

    // Helpers
    void Clear();
    void Slice_Run(ffv1_frame_slot* Slot, slice* Slice);
    void Frame_End(ffv1_frame_slot* Slot);
    size_t Slots_Busy();
};

//---------------------------------------------------------------------------
#endif
