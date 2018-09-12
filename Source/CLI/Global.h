/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef GlobalH
#define GlobalH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "CLI/Config.h"
#include <map>
#include <vector>
#include <string>
using namespace std;
//---------------------------------------------------------------------------

class global
{
public:
    // Options
    map<string, string>         VideoInputOptions;
    map<string, string>         OutputOptions;
    size_t                      AttachmentMaxSize;
    string                      rawcooked_reversibility_data_FileName;
    string                      OutputFileName;
    string                      BinName;
    bool                        DisplayCommand;
    bool                        AcceptFiles;
    bool                        HasAtLeastOneDir;
    bool                        HasAtLeastOneFile;

    // Intermediate info
    size_t                      Path_Pos_Global;
    vector<string>              Inputs;

    // Options
    int ManageCommandLine(const char* argv[], int argc);
    int SetDefaults();

private:
    int SetOption(const char* argv[], int& i, int argc);
    int SetOutputFileName(const char* FileName);
    int SetBinName(const char* FileName);
    int SetDisplayCommand();
    int SetAcceptFiles();
};

#endif
