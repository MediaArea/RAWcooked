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
    int                         Close();

private:
    void*                       Private;
};

#endif
