/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef TrackH
#define TrackH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include "Lib/Utils/FileIO/FileIO.h"
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
format Format_FromCodecID(const char* Name);
format_kind FormatKind(format Format);
base_wrapper* CreateWrapper(format Format, ThreadPool* Pool);
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

    bool                        Init(const uint8_t* BaseData);
    input_base_uncompressed*    InitOutput_Find();
    input_base_uncompressed*    InitOutput(input_base_uncompressed* PotentialParser, raw_frame::flavor Flavor);
    bool                        Process(const uint8_t* Data, size_t Size);
    bool                        OutOfBand(const uint8_t* Data, size_t Size);
    void                        End(size_t i);

    void                        SetFormat(const char* NewFormat) { Format = Format_FromCodecID(NewFormat); }
    void                        SetWidth(uint32_t NewWidth) { Width = NewWidth; }
    void                        SetHeight(uint32_t NewHeight) { Height = NewHeight; }

private:
    ThreadPool*                 Pool = nullptr;
    frame_writer*               FrameWriter = nullptr;
    input_base_uncompressed*    DecodedFrameParser = nullptr;
    base_wrapper*               Wrapper = nullptr;
    raw_frame*                  RawFrame = nullptr;
    format                      Format = format::None;
    uint32_t                    Width = 0;
    uint32_t                    Height = 0;

    void                        ParseBuffer() {}
    void                        BufferOverflow() {}
    void                        Undecodable(reversibility_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
};

#endif
