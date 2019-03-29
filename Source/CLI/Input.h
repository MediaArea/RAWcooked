/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef InputH
#define InputH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "CLI/Config.h"
#include "CLI/Global.h"
#include <string>
#include <vector>
using namespace std;
//---------------------------------------------------------------------------

class input
{
public:
    vector<string>              Files;

    // Commands
    int AnalyzeInputs(global& Global);
    void DetectSequence(bool CheckIfFilesExist, size_t AllFiles_Pos, vector<string>& RemovedFiles, size_t& Path_Pos, string& FileName_Template, string& FileName_StartNumber, string& FileName_EndNumber, errors* Errors = nullptr);
};

#endif
