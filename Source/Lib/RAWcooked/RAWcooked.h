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
#include <condition_variable>
#include <cstdint>
#include <cstddef>
#include <string>
using namespace std;
class ebml_writer;
//---------------------------------------------------------------------------

class rawcooked
{
public:
                                rawcooked();
                                ~rawcooked();

    bool                        Unique; // If set, data is for the whole stream (unique file)

    uint8_t*                    BeforeData;
    uint64_t                    BeforeData_Size;

    uint8_t*                    AfterData;
    uint64_t                    AfterData_Size;

    uint8_t*                    InData;
    uint64_t                    InData_Size;

    md5*                        HashValue = nullptr;
    bool                        IsAttachment = false;

    void                        Parse();
    void                        ResetTrack();
    void                        Close();
    void                        Delete();

    string                      FileName;
    string                      OutputFileName;
    uint64_t                    FileSize;

    // Erasure
    struct hash_value
    {
        unsigned char Values[16];
    };
    void                        Erasure(hash_value* Hashes_Values, uint8_t* ParityShards, size_t Buffer_Size);
    void                        Erasure_AppendToFile();

    // Errors
    user_mode*                  Mode = nullptr;
    ask_callback                Ask_Callback = nullptr;
    bool*                       ProgressIndicator_IsPaused = nullptr;
    condition_variable*         ProgressIndicator_IsEnd = nullptr;
    errors*                     Errors = nullptr;

private:
    // File IO
    file                        File;
    size_t                      BlockCount;
    bool                        File_WasCreated = false;
    void WriteToDisk(uint8_t* Buffer, size_t Buffer_Size, bool Append = false);

    // First frame info
    uint8_t*                    FirstFrame_Before;
    size_t                      FirstFrame_Before_Size;
    uint8_t*                    FirstFrame_After;
    size_t                      FirstFrame_After_Size;
    uint8_t*                    FirstFrame_FileName;
    size_t                      FirstFrame_FileName_Size;

    // Erasure
    ebml_writer*                EbmlWriter = nullptr;
};

//---------------------------------------------------------------------------
#endif
