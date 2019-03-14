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
#include "CLI/Config.h"
#include "CLI/Global.h"
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
                                filemap();
                                ~filemap();

    // Direct access to file map data
    unsigned char*              Buffer;
    size_t                      Buffer_Size;

    // Actions
    int                         Open_ReadMode(const char* FileName);
    int                         Open_ReadMode(const string& FileName) { return Open_ReadMode(FileName.c_str()); }
    int                         Remap();
    int                         Close();

private:
    void*                       Private;
};

class file
{
public:
    // Constructor/Destructor
                                ~file() { Close(); }

    // Actions
    bool                        Open(const string& BaseDirectory_Source, const string& OutputFileName_Source, const char* Mode);
    bool                        IsOpen() { return Private ? true : false; }
    bool                        Write(const void* Buffer, size_t Size);
    bool                        Close();

    // Errors
    errors*                     Errors = NULL;

private:
    void*                       Private = NULL;
    string                      OutputFileName;
};

#endif
