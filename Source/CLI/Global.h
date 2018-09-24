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
#include <condition_variable>
#include <thread>
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
    bool                        Quiet;

    // Intermediate info
    size_t                      Path_Pos_Global;
    vector<string>              Inputs;

    // Options
    int ManageCommandLine(const char* argv[], int argc);
    int SetDefaults();

    // Progress indicator
    void                        ProgressIndicator_Start(size_t Total);
    void                        ProgressIndicator_Increment() { ProgressIndicator_Current++; }
    void                        ProgressIndicator_Stop();

    // Theading relating functions
    void                        ProgressIndicator_Show();

private:
    int SetOption(const char* argv[], int& i, int argc);
    int SetOutputFileName(const char* FileName);
    int SetBinName(const char* FileName);
    int SetDisplayCommand();
    int SetAcceptFiles();

    // Progress indicator
    condition_variable          ProgressIndicator_IsEnd;
    thread*                     ProgressIndicator_Thread;
    size_t                      ProgressIndicator_Current;
    size_t                      ProgressIndicator_Total;
};

#endif
