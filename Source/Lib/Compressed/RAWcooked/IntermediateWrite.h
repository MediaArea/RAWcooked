/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef IntermediateWriteH
#define IntermediateWriteH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Utils/Errors/Errors.h"
#include "Lib/Utils/FileIO/FileIO.h"
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
    bool                        Rename(const string& NewwFileName);

    string                      FileName;

    // Errors
    user_mode*                  Mode = nullptr;
    ask_callback                Ask_Callback = nullptr;
    bool*                       ProgressIndicator_IsPaused = nullptr;
    condition_variable*         ProgressIndicator_IsEnd = nullptr;
    errors*                     Errors = nullptr;
    void                        SetErrorFileBecomingTooBig();

    // File IO
    void WriteToDisk(const uint8_t* Buffer, size_t Buffer_Size);

protected:
    // File IO
    file                        File;
    bool                        File_WasCreated = false;
};

//---------------------------------------------------------------------------
#endif
