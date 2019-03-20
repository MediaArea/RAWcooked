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
#include "Lib/Errors.h"
#include "Lib/FileIO.h"
#include <cstdint>
#include <cstddef>
#include <string>
using namespace std;
//---------------------------------------------------------------------------

class rawcooked
{
public:
                                rawcooked();
                                ~rawcooked();

    bool                        Unique; // If set, data is for the whole stream (unique file)

    uint8_t*                    Before;
    uint64_t                    Before_Size;

    uint8_t*                    After;
    uint64_t                    After_Size;

    uint8_t*                    In;
    uint64_t                    In_Size;

    void                        Parse();
    void                        ResetTrack();
    void                        Close();
    void                        Delete();

    string                      FileName;
    string                      OutputFileName;
    uint64_t                    FileSize;

    // Errors
    user_mode*                  Mode = NULL;
    ask_callback                Ask_Callback = NULL;
    errors*                     Errors = NULL;

private:
    // File IO
    file                        File;
    size_t                      BlockCount;
    bool                        File_WasCreated = false;
    void WriteToDisk(uint8_t* Buffer, size_t Buffer_Size);

    // First frame info
    uint8_t*                    FirstFrame_Before;
    size_t                      FirstFrame_Before_Size;
    uint8_t*                    FirstFrame_After;
    size_t                      FirstFrame_After_Size;
    uint8_t*                    FirstFrame_FileName;
    size_t                      FirstFrame_FileName_Size;
};

//---------------------------------------------------------------------------
#endif
