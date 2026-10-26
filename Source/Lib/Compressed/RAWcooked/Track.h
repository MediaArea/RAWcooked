/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef TrackH
#define TrackH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <bitset>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include "Lib/Utils/FileIO/FileIO.h"
class base_wrapper;
class data_holder;
class frame_writer;
struct output_item;
struct output_pending;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace reversibility_issue {

    namespace undecodable
    {

        enum code : uint8_t
        {
            ReversibilityData_UnreadableFrameHeader,
            ReversibilityData_FrameCount,
            Max
        };

    } // unparsable

} // reversibility_issue

//---------------------------------------------------------------------------
ENUM_BEGIN(format)
    None,
    FFV1,
    FLAC,
    PCM,
    VFW, // Temporary, needs out of band data for knowing the real format
ENUM_END(format)

//---------------------------------------------------------------------------
enum class format_kind
{
    unknown,
    video,
    audio,
};

//---------------------------------------------------------------------------
::format Format_FromCodecID(const char* Name);
format_kind FormatKind(::format Format);
base_wrapper* CreateWrapper(::format Format, ThreadPool* Pool, bool UseInternalDecoder = false, bool UseFFmpegDecoder = false, bool UseVulkanDecoder = false, bool UseAutoDecoder = false, bool AllowVulkanCPU = false);
class reversibility;

//---------------------------------------------------------------------------
class base_wrapper;
class track_info : public input_base
{
public:
    reversibility*         ReversibilityData = nullptr;

    track_info(const frame_writer& FrameWriter_Source, const bitset<Action_Max>& Actions, errors* Errors, ThreadPool* Pool);
    track_info(const frame_writer* FrameWriter_Source, const bitset<Action_Max>& Actions, errors* Errors, ThreadPool* Pool) :
        track_info(*FrameWriter_Source, Actions, Errors, Pool)
    {
    }
    ~track_info();

    bool                        ParseDecodedFrame(input_base_uncompressed* Parser = nullptr);
    bool                        ParseDecodedFrame(const buffer& BeforeData, const buffer& AfterData, uint64_t FileSize); // Without the decoded content, which is not needed if the file size is known

    bool                        Init(const uint8_t* BaseData);
    input_base_uncompressed*    InitOutput_Find();
    input_base_uncompressed*    InitOutput(input_base_uncompressed* PotentialParser, raw_frame::flavor Flavor);
    bool                        Process(const uint8_t* Data, size_t Size, data_holder* Holder);
    bool                        OutOfBand(const uint8_t* Data, size_t Size);
    void                        End(size_t i);

    void                        SetFormat(const char* NewFormat) { Format = Format_FromCodecID(NewFormat); }
    void                        SetWidth(uint32_t NewWidth) { Width = NewWidth; }
    void                        SetHeight(uint32_t NewHeight) { Height = NewHeight; }
    size_t                      Padding(); // Count of bytes the decoder reads after the frame data

    // Frame threading: wait for the end of the output of the last frame
    void                        Output_Wait();
    void                        Output_End(); // No more frames
    bool                        Output_IsShared(); // Output may share data with other tracks (e.g. compound file)

    // Late updates
    void                        UpdateReversibility(reversibility* Reversibility, input_base_uncompressed_compound* Compound, raw_frame::flavor Flavor);

private:
    ThreadPool*                 Pool = nullptr;
    frame_writer*               FrameWriter = nullptr;
    input_base_uncompressed*    DecodedFrameParser = nullptr;
    base_wrapper*               Wrapper = nullptr;
    raw_frame*                  RawFrame = nullptr;
    ::format                    Format = format::None;
    uint32_t                    Width = 0;
    uint32_t                    Height = 0;

    // Frame threading: decoding of frames during the output of previous frames (if the wrapper can split), outputs in parallel if frames are in different files
    output_pending*             Output_Pending = nullptr;
    bool                        Process_Split(const uint8_t* Data, size_t Size, data_holder* Holder);
    void                        Output_Run();
    void                        Output(output_item* Item, frame_writer* Writer);
    void                        Parse_Flush(bool Force);

    // Decoded frame parsing without the decoded content, buffer with zeroes kept between frames
    buffer                      ParseDecodedFrame_Buffer;
    size_t                      ParseDecodedFrame_BeforeSize = 0;
    size_t                      ParseDecodedFrame_AfterSize = 0;

    void                        ParseBuffer() {}
    void                        BufferOverflow() {}
    void                        Undecodable(reversibility_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
};

#endif
