/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
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
    void DetectSequence(bool CheckIfFilesExist, size_t AllFiles_Pos, vector<string>& RemovedFiles, size_t& Path_Pos, string& FileName_Template, string& FileName_StartNumber, string& FileName_EndNumber, string& FileList, bitset<Action_Max> const& Actions, errors* Errors = nullptr);

    // Checks
    static void CheckDurations(vector<double> const& Durations, vector<string> const& Durations_FileName, errors* Errors = nullptr);

    // I/O
    static bool OpenInput(filemap& FileMap, const string& Name, errors* Errors);
};

#endif
