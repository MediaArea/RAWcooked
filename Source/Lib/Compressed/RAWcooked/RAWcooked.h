/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef RawCookedH
#define RawCookedH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Compressed/RAWcooked/IntermediateWrite.h"
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <string>
using namespace std;
//---------------------------------------------------------------------------

struct parse_params
{
    bool                        Unique = false; // If set, data is for the whole stream (unique file)

    const uint8_t*              BeforeData = nullptr;
    uint64_t                    BeforeData_Size = 0;

    const uint8_t*              AfterData = nullptr;
    uint64_t                    AfterData_Size = 0;

    const uint8_t*              InData = nullptr;
    uint64_t                    InData_Size = 0;

    md5*                        HashValue = nullptr;
    bool                        IsAttachment = false;
    bool                        IsContainer = false;

    string                      InputFile_Name;
    uint64_t                    InputFile_Size = (uint64_t)-1;
};

class rawcooked : public intermediate_write
{
public:
                                rawcooked();
                                ~rawcooked();

    void                        Parse(const parse_params& Params = {});
    void                        ResetTrack();

    bool                        HasInData();

    filemap*                    ReversibilityFile = nullptr;
    enum class version
    {
        v1,
        mini,
        v2,
    };
    version                     Version = version::v1;

private:
    // Private
    class private_data;
    private_data* const          Data_;
};

//---------------------------------------------------------------------------
#endif
