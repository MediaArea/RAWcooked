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
#include "Lib/Config.h"
#include "Lib/License.h"
#include <map>
#include <vector>
#include <string>
#include <condition_variable>
#include <thread>
#include <bitset>
using namespace std;
//---------------------------------------------------------------------------

//--------------------------------------------------------------------------
user_mode Ask_Callback(user_mode* Mode, const string& FileName, bool Always);

//--------------------------------------------------------------------------
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
    string                      LicenseKey;
    bool                        IgnoreLicenseKey;
    bool                        ShowLicenseKey;
    bool                        StoreLicenseKey;
    bool                        DisplayCommand;
    bool                        AcceptFiles;
    bool                        HasAtLeastOneDir;
    bool                        HasAtLeastOneFile;
    bool                        OutputFileName_IsProvided;
    bool                        Quiet;
    bool                        Check;
    bitset<Action_Max>          Actions;

    // Intermediate info
    size_t                      Path_Pos_Global;
    vector<string>              Inputs;
    license                     License;
    user_mode                        Mode = Ask;
    errors                      Errors;

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
    int SetLicenseKey(const char* Key, bool Add);
    int SetDisplayCommand();
    int SetAcceptFiles();
    int SetCheck(bool Value);
    int SetCheck(const char* Value, int& i);
    int SetCheckPadding(bool Value);
    int SetConch(bool Value);
    int SetEncode(bool Value);

    // Progress indicator
    condition_variable          ProgressIndicator_IsEnd;
    thread*                     ProgressIndicator_Thread;
    size_t                      ProgressIndicator_Current;
    size_t                      ProgressIndicator_Total;
};

#endif
