/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef IntermediateWriteH
#define IntermediateWriteH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Errors.h"
#include "Lib/FileIO.h"
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <string>
using namespace std;
//---------------------------------------------------------------------------

class intermediate_write
{
public:
                                intermediate_write();
                                ~intermediate_write();

    bool                        Close();
    bool                        Delete();

    string                      FileName;

    // Errors
    user_mode*                  Mode = nullptr;
    ask_callback                Ask_Callback = nullptr;
    bool*                       ProgressIndicator_IsPaused = nullptr;
    condition_variable*         ProgressIndicator_IsEnd = nullptr;
    errors*                     Errors = nullptr;

    // File IO
    void WriteToDisk(uint8_t* Buffer, size_t Buffer_Size);

protected:
    // File IO
    file                        File;
    bool                        File_WasCreated = false;
};

//---------------------------------------------------------------------------
#endif
