/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef CoreH
#define CoreH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Core/Config.h"
#include "Lib/Core/Global.h"
#include "Lib/Core/Input.h"
#include "Lib/Core/Output.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
#include <string>
#include <vector>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
struct parse_info;

//---------------------------------------------------------------------------
class core
{
public:
    // Configuration (callbacks and options not from the command line are set before Run()) and results
    global                      Global;
    input                       Input;
    output                      Output;

    // Commands
    // Several runs in a process (a core per run) are independent, except that the FFmpeg libraries stay loaded: another libavcodec library provided to a later run is not used
    int                         Run(const char* argv[], int argc); // Whole processing; options and inputs as in a command line, argv[0] is the name of the program. Returns the exit code

private:
    // Reversibility data, after Global (so destroyed before it): it has pointers to members of Global
    rawcooked                   RAWcooked;

    // Parsing of the inputs
    bool                        ParseFile_Input(input_base& SingleFile, filemap& FileMap, input_info* InputInfo, bool OverrideCheckPadding = false);
    bool                        ParseFile_AdditionalInput(input_base_uncompressed& S, filemap& FileMap, const vector<FileEntry>& RemovedFiles, bool OverrideCheckPadding, size_t HeaderSize, const directory* Dir, size_t i, const sequence_format* First);
    bool                        ParseFile_AdditionalInputs(input_base_uncompressed& SingleFile, filemap& FileMap, const vector<FileEntry>& RemovedFiles, bool OverrideCheckPadding);
    int                         ParseFile_Uncompressed(parse_info& ParseInfo, size_t Files_Pos);
    int                         ParseFile_Compressed(parse_info& ParseInfo, const string* FileOpenName);
    int                         ParseFile(size_t Files_Pos);
    friend struct parse_info;
};

//---------------------------------------------------------------------------
int Help_Edit(); // List of the edit keys of all formats (used by the command line management)

#endif
