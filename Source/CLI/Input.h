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

// File entry with size from stat/FindFirstFile to avoid redundant stat/GetFileSize calls
struct FileEntry
{
    string FileName;
    uintmax_t FileSize;
};

class input
{
public:
    vector<FileEntry>           Files;

    // Commands
    int AnalyzeInputs(global& Global);
    void DetectSequence(bool CheckIfFilesExist, size_t AllFiles_Pos, vector<FileEntry>& RemovedFiles, size_t& Path_Pos, string& FileName_Template, string& FileName_StartNumber, string& FileName_EndNumber, string& FileList, bitset<Action_Max> const& Actions, errors* Errors = nullptr);

    // Checks
    static void CheckDurations(vector<double> const& Durations, vector<string> const& Durations_FileName, errors* Errors = nullptr);

    // Order of file names, negative if Left is before Right, positive if Left is after Right, 0 if same
    static int FileName_Compare(const string& Left, const string& Right);

    // I/O
    static bool OpenInput(filemap& FileMap, const string& Name, size_t FileSize, errors* Errors, bool AlsoWrite = false, size_t HeaderSize = 0, const directory* Dir = nullptr); // Only the header is read if HeaderSize is not 0, the file is opened relative to Dir if it is directly in it
};

#endif
