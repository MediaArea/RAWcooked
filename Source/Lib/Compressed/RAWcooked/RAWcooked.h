/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
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

class rawcooked : public intermediate_write
{
public:
                                rawcooked();
                                ~rawcooked();

    bool                        Unique = false; // If set, data is for the whole stream (unique file)

    const uint8_t*              BeforeData = nullptr;
    uint64_t                    BeforeData_Size = 0;

    const uint8_t*              AfterData = nullptr;
    uint64_t                    AfterData_Size = 0;

    const uint8_t*              InData = nullptr;
    uint64_t                    InData_Size = 0;

    md5*                        HashValue = nullptr;
    bool                        IsAttachment = false;

    void                        Parse();
    void                        ResetTrack();

    string                      OutputFileName;
    uint64_t                    FileSize = 0;

private:
    // Private
    class private_data;
    private_data* const          Data_;
};

//---------------------------------------------------------------------------
#endif
