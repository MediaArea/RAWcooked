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

    bool                        Unique; // If set, data is for the whole stream (unique file)

    const uint8_t*              BeforeData;
    uint64_t                    BeforeData_Size;

    const uint8_t*              AfterData;
    uint64_t                    AfterData_Size;

    const uint8_t*              InData;
    uint64_t                    InData_Size;

    md5*                        HashValue = nullptr;
    bool                        IsAttachment = false;

    void                        Parse();
    void                        ResetTrack();

    string                      OutputFileName;
    uint64_t                    FileSize;

private:
    // File IO
    size_t                      BlockCount;

    // First frame info
    buffer                      FirstFrame_Before;
    buffer                      FirstFrame_After;
    buffer                      FirstFrame_FileName;
};

//---------------------------------------------------------------------------
#endif
