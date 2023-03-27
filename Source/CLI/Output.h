/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef OutputH
#define OutputH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "CLI/Config.h"
#include "CLI/Global.h"
#include <string>
#include <vector>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
struct stream
{
    string                      FileName;
    string                      FileName_Template;
    string                      FileName_StartNumber;
    string                      FileName_EndNumber;
    string                      FileList;
    string                      Flavor;
    string                      Slices;
    string                      FrameRate;
    size_t                      StreamCountMinus1 = 0;
    bool                        IsContainer = false;
    bool                        Problem = false;
};
struct attachment
{
    string                      FileName_In; // Complete path
    string                      FileName_Out; // Relative path
};

class output
{
public:
    // To be filled by external means
    vector<stream>              Streams;
    vector<attachment>          Attachments;

    // Commands
    int Process(global& Global, bool IgnoreReversibilityFile = false);

private:
    int FFmpeg_Command(const char* FileName, global& Global, bool IgnoreReversibilityFile = false);
};

#endif
