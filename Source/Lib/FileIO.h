/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FileIOH
#define FileIOH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Errors.h"
#include "Lib/RawFrame/RawFrame.h"
#include <string>
#include <vector>
using namespace std;
//---------------------------------------------------------------------------

class filemap
{
public:
    // Constructor/Destructor
                                ~filemap() { Close(); }

    // Direct access to file map data
    unsigned char*              Buffer = NULL;
    size_t                      Buffer_Size = 0;

    // Actions
    int                         Open_ReadMode(const char* FileName);
    int                         Open_ReadMode(const string& FileName) { return Open_ReadMode(FileName.c_str()); }
    bool                        IsOpen() { return Private == (void*)-1 ? false : true; }
    int                         Remap();
    int                         Close();

private:
    void*                       Private = (void*)-1;
    #if defined(_WIN32) || defined(_WINDOWS)
    void*                       Private2 = NULL;
    #endif //defined(_WIN32) || defined(_WINDOWS)
};

class file
{
public:
    // Constructor/Destructor
                                ~file() { Close(); }

    // Actions
    enum return_value
    {
        OK = 0,
        Error_CreateDirectory,
        Error_FileCreate,
        Error_FileAlreadyExists,
        Error_FileWrite,
        Error_Seek,
    };
    return_value                Open_WriteMode(const string& BaseDirectory_Source, const string& OutputFileName_Source, bool RejectIfExists = false, bool Truncate = false);
    bool                        IsOpen() { return Private == (void*)-1 ? false : true; }
    return_value                Write(const uint8_t* Buffer, size_t Size);
    enum seek_value
    {
        Begin,
        Current,
        End,
    };
    return_value                Seek(int64_t Offset, seek_value Method = Begin);
    return_value                SetEndOfFile();
    return_value                Close();

private:
    void*                       Private = (void*)-1;
    string                      OutputFileName;
};

#endif
