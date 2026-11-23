/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include <iostream>
#include <string>
#include "Lib/Core/Core.h"
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
core Core;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Ask user about overwriting files
user_mode Ask_Callback(user_mode* Mode, const string& FileName, const string& ExtraText, bool Always, bool* ProgressIndicator_IsPaused, condition_variable* ProgressIndicator_IsEnd)
{
    if (Mode && *Mode != Ask)
        return *Mode;

    string Result;
    if (ProgressIndicator_IsPaused)
        *ProgressIndicator_IsPaused = true;
    if (ProgressIndicator_IsEnd)
        ProgressIndicator_IsEnd->notify_one();
    cerr << "                                                            \r"; // Clean up output
    cerr << "File '" << FileName << "' already exists" << ExtraText << ". Overwrite? [y/N] ";
    getline(cin, Result);
    if (ProgressIndicator_IsPaused)
        *ProgressIndicator_IsPaused = false;
    if (ProgressIndicator_IsEnd)
        ProgressIndicator_IsEnd->notify_one();
    cerr << "                                                            \r"; // Clean up output
    user_mode NewMode = (!Result.empty() && (Result[0] == 'Y' || Result[0] == 'y')) ? AlwaysYes : AlwaysNo;

    if (Mode && Always)
    {
        cerr << "Use this choice for all other files? [y/N] ";
        getline(cin, Result);
        if (!Result.empty() && (Result[0] == 'Y' || Result[0] == 'y'))
            * Mode = NewMode;
    }

    cerr << endl;
    return NewMode;
}

//---------------------------------------------------------------------------
// Ask user a question with a yes or no answer
bool Question_Callback(const string& Question)
{
    cerr << Question << " [y/N] ";
    string Result;
    getline(cin, Result);
    return !Result.empty() && (Result[0] == 'Y' || Result[0] == 'y');
}

//---------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
    Core.Global.Ask_Callback = Ask_Callback;
    Core.Global.Question_Callback = Question_Callback;
    return Core.Run(argv, argc);
}
