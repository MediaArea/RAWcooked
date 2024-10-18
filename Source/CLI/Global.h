/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
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
#include "Lib/Common/Common.h"
#include "Lib/Uncompressed/HashSum/HashSum.h"
#include "Lib/License/License.h"
#include <map>
#include <vector>
#include <string>
#include <thread>
#include <bitset>
using namespace std;
//---------------------------------------------------------------------------

//--------------------------------------------------------------------------
class global
{
public:
    // Options
    map<string, string>         VideoInputOptions;
    map<string, string>         OutputOptions;
    vector<string>              MoreOutputOptions;
    size_t                      AttachmentMaxSize;
    string                      rawcooked_reversibility_FileName;
    string                      OutputFileName;
    string                      FrameMd5FileName;
    string                      BinName;
    string                      LicenseKey;
    uint64_t                    SubLicenseId;
    uint64_t                    SubLicenseDur;
    filemap::method             FileOpenMethod;
    bool                        IgnoreLicenseKey;
    bool                        ShowLicenseKey;
    bool                        StoreLicenseKey;
    bool                        DisplayCommand;
    bool                        AcceptFiles;
    bool                        HasAtLeastOneDir;
    bool                        HasAtLeastOneFile;
    bool                        OutputFileName_IsProvided;
    bool                        Quiet;
    bitset<Action_Max>          Actions;

    // Intermediate info
    size_t                      Path_Pos_Global;
    vector<string>              Inputs;
    license                     License;
    user_mode                   Mode = Ask;
    hashes                      Hashes;
    errors                      Errors;
    ask_callback                Ask_Callback = nullptr;

    // Conformance check intermediary info
    vector<double>              Durations;
    vector<string>              Durations_FileName;

    // Options
    int ManageCommandLine(const char* argv[], int argc);
    int SetDefaults();

    // Progress indicator
    void                        ProgressIndicator_Start(size_t Total);
    void                        ProgressIndicator_Increment() { ProgressIndicator_Current++; }
    void                        ProgressIndicator_Stop();
    condition_variable          ProgressIndicator_IsEnd;
    bool                        ProgressIndicator_IsPaused = false;

    // Theading relating functions
    void                        ProgressIndicator_Show();

    int SetCheck(bool Value);
    int SetVersion(const char* Value);
    int SetOption(const char* argv[], int& i, int argc);
    int SetOutputFileName(const char* FileName);
    int SetBinName(const char* FileName);
    int SetLicenseKey(const char* Key, bool Add);
    int SetSubLicenseId(uint64_t Id);
    int SetSubLicenseDur(uint64_t Dur);
    int SetDisplayCommand();
    int SetAcceptFiles();
    int SetCheck(const char* Value, int& i);
    int SetQuickCheck();
    int SetCheckPadding(bool Value);
    int SetQuickCheckPadding();
    int SetAcceptGaps(bool Value);
    int SetCoherency(bool Value);
    int SetConch(bool Value);
    int SetDecode(bool Value);
    int SetEncode(bool Value);
    int SetInfo(bool Value);
    int SetFrameMd5(bool Value);
    int SetFrameMd5An(bool Value);
    int SetFrameMd5FileName(const char* FileName);
    int SetHash(bool Value);
    int SetFileOpenMethod(const char* Value);
    int SetAll(bool Value);

private:
    // Progress indicator
    thread*                     ProgressIndicator_Thread;
    size_t                      ProgressIndicator_Current;
    size_t                      ProgressIndicator_Total;

    // Temp
    bool                        NextFileNameIsOutput = false;
    bool                        OptionsForOtherFiles = false;
};

#endif
