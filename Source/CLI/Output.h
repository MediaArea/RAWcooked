/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
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
    bool                        Problem;

    stream()
        : Problem(false)
    {}
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
    int Process(global& Global);

private:
    int FFmpeg_Command(const char* FileName, global& Global);
};

#endif
