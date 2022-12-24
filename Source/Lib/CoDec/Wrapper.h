/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
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
class base_wrapper
{
public:
    virtual ~base_wrapper() {}

    // Actions
    virtual void                Process(const uint8_t* Data, size_t Size) = 0;
    inline virtual void         OutOfBand(const uint8_t* /*Data*/, size_t /*Size*/) {};

public:
    raw_frame*                  RawFrame = nullptr;
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
