/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef WrapperH
#define WrapperH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include "Lib/Utils/FileIO/FileIO.h"
#include "Lib/Compressed/Matroska/Matroska.h"
#include "Lib/Compressed/RAWcooked/Track.h"
#include <bitset>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
class base_wrapper;
class frame_writer;
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Holder of the frame data given to a decoder, valid during the call with the padding requested by the decoder (Padding()) readable after it
// The decoder may use the data after the call: Hold() (during the call) returns the data with the padding (the same data or a copy), valid until Release() (from any thread)
// Allocated: the data is needed in allocated memory, not in a memory map of a file (e.g. data imported in GPU memory space)
class data_holder
{
public:
    virtual const uint8_t*      Hold(bool Allocated = false) = 0;
    virtual void                Release() = 0;

protected:
    virtual                     ~data_holder() {}
};

//---------------------------------------------------------------------------
class base_wrapper
{
public:
    virtual ~base_wrapper() {}

    // Actions
    inline virtual bool         Process(const uint8_t* /*Data*/, size_t /*Size*/, data_holder* /*Holder*/) { return true; } // Not used if CanSplit(), Holder: for using the data after the call (nullptr if not possible)
    inline virtual bool         OutOfBand(const uint8_t* /*Data*/, size_t /*Size*/) { return true; };
    inline virtual size_t       Padding() { return 0; } // Count of bytes the decoder reads after the frame data

    // Process in 2 steps if CanSplit(): Decode() starts the decoding of a frame to Frame, then FrameReady->Ready(Opaque, Error) is called when the frame is decoded
    // Ready() may be called from any thread, before the end of Decode(), and not in the order of Decode() calls
    inline virtual bool         CanSplit() { return false; }
    inline virtual void         Decode(const uint8_t* /*Data*/, size_t /*Size*/, data_holder* /*Holder*/, raw_frame* /*Frame*/, void* Opaque) { FrameReady->Ready(Opaque, true); }
    inline virtual size_t       Depth() { return 2; } // Max count of frames between Decode() and the end of their use by the caller

public:
    raw_frame*                  RawFrame = nullptr; // Decoded frame if not split, else frame with the configuration of decoded frames (flavor)
    raw_frame_ready*            FrameReady = nullptr; // Receiver of decoded frames, if split
};

//---------------------------------------------------------------------------
class video_wrapper : public base_wrapper
{
public:
    // Config
    virtual void                SetWidth(uint32_t /*Width*/) {};
    virtual void                SetHeight(uint32_t /*Height*/) {};
};

//---------------------------------------------------------------------------
class audio_wrapper : public base_wrapper
{
public:
    // Config
    void                        SetConfig(uint8_t BitDepth, sign Sign, endianness Endianness);

protected:
    uint8_t                     OutputBitDepth = 0;
    union sign_or_endianness
    {
        sign                    Sign = sign::U;
        endianness              Endianness;
    };
    sign_or_endianness          SignOrEndianess;
};

#endif
