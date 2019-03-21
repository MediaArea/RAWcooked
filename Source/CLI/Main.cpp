/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
*
*  Use of this source code is governed by a BSD-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#include "CLI/Global.h"
#include "CLI/Input.h"
#include "CLI/Output.h"
#include "Lib/FileIO.h"
#include "Lib/Matroska/Matroska_Common.h"
#include "Lib/DPX/DPX.h"
#include "Lib/TIFF/TIFF.h"
#include "Lib/WAV/WAV.h"
#include "Lib/AIFF/AIFF.h"
#include "Lib/FFV1/FFV1_Frame.h"
#include "Lib/RawFrame/RawFrame.h"
#include "Lib/RAWcooked/RAWcooked.h"
#include <map>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <algorithm>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
global Global;
input Input;
output Output;
rawcooked RAWcooked;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Ask user about overwriting files
user_mode Ask_Callback(user_mode* Mode, const string& FileName, const string& ExtraText, bool Always)
{
    if (Mode && *Mode != Ask)
        return *Mode;

    cerr << "File '" << FileName << "' already exists" << ExtraText << ". Overwrite ? [y/N] ";
    string Result;
    getline(cin, Result);
    user_mode NewMode = (!Result.empty() && (Result[0] == 'Y' || Result[0] == 'y')) ? AlwaysYes : AlwaysNo;

    if (Mode && Always)
    {
        cerr << "Use this choice for all other files ? [y/N] ";
        getline(cin, Result);
        if (!Result.empty() && (Result[0] == 'Y' || Result[0] == 'y'))
            * Mode = NewMode;
    }

    cerr << endl;
    return NewMode;
}

//---------------------------------------------------------------------------
struct parse_info
{
    string* Name;
    filemap FileMap;
    vector<string> RemovedFiles;
    string FileName_Template;
    string FileName_StartNumber;
    string FileName_EndNumber;
    string Flavor;
    string Slices;
    string FrameRate;
    bool   IsDetected;
    bool   Problem;

    bool ParseFile_Input(input_base& Input);
    bool ParseFile_Input(input_base_uncompressed& SingleFile, input& Input, size_t Files_Pos);

    parse_info():
        IsDetected(false),
        Problem(false)
    {}
};

//---------------------------------------------------------------------------
bool parse_info::ParseFile_Input(input_base& SingleFile)
{
    // Init
    SingleFile.AcceptTruncated = false;
    SingleFile.CheckPadding = Global.CheckPadding;

    // Parse
    SingleFile.Parse(FileMap);
    Global.ProgressIndicator_Increment();

    // Management
    if (SingleFile.IsDetected())
        IsDetected = true;

    if (Global.Errors.HasErrors())
        return true;
    return false;
}

//---------------------------------------------------------------------------
bool parse_info::ParseFile_Input(input_base_uncompressed& SingleFile, input& Input, size_t Files_Pos)
{
    if (IsDetected)
        return false;

    // Init
    RAWcooked.Mode = &Global.Mode;
    RAWcooked.Ask_Callback = Ask_Callback;
    RAWcooked.Errors = &Global.Errors;
    SingleFile.RAWcooked = &RAWcooked;
    RAWcooked.OutputFileName = Name->substr(Global.Path_Pos_Global);

    // Parse
    if (ParseFile_Input((input_base&)SingleFile))
        return true;
    if (!IsDetected)
        return false;

    // Management
    Flavor = SingleFile.Flavor_String();
    if (SingleFile.IsSequence)
        Input.DetectSequence(Global.HasAtLeastOneFile, Files_Pos, RemovedFiles, Global.Path_Pos_Global, FileName_Template, FileName_StartNumber, FileName_EndNumber);
    if (RemovedFiles.empty())
        RemovedFiles.push_back(*Name);
    else
    {
        Global.ProgressIndicator_Start(Input.Files.size() + RemovedFiles.size() - 1);
        size_t i_Max = RemovedFiles.size();
        for (size_t i = 1; i < i_Max; i++)
        {
            Name = &RemovedFiles[i];
            FileMap.Open_ReadMode(*Name);
            RAWcooked.OutputFileName = Name->substr(Global.Path_Pos_Global);

            if (ParseFile_Input((input_base&)SingleFile))
                return true;
        }

    }

    // License
    if (!Problem)
        Problem = !Global.License.IsSupported(SingleFile.ParserCode, SingleFile.Flavor);

    return false;
}

//---------------------------------------------------------------------------
int ParseFile_Uncompressed(parse_info& ParseInfo, size_t Files_Pos)
{
    // Init
    RAWcooked.ResetTrack();
    map<string, string>::iterator FrameRateFromOptions = Global.VideoInputOptions.find("framerate");
    if (FrameRateFromOptions != Global.VideoInputOptions.end())
        ParseInfo.FrameRate = FrameRateFromOptions->second;

    // WAV
    if (!ParseInfo.IsDetected)
    {
        wav WAV(&Global.Errors);
        if (ParseInfo.ParseFile_Input(WAV, Input, Files_Pos))
            return 1;
    }

    // AIFF
    if (!ParseInfo.IsDetected)
    {
        aiff AIFF(&Global.Errors);
        if (ParseInfo.ParseFile_Input(AIFF, Input, Files_Pos))
            return 1;
    }

    // DPX
    if (!ParseInfo.IsDetected)
    {
        dpx DPX(&Global.Errors);
        DPX.FrameRate = ParseInfo.FrameRate.empty() ? &ParseInfo.FrameRate : NULL;
        if (ParseInfo.ParseFile_Input(DPX, Input, Files_Pos))
            return 1;

        if (ParseInfo.IsDetected)
        {
            stringstream t;
            t << DPX.slice_x * DPX.slice_y;
            ParseInfo.Slices = t.str();
        }
    }

    // TIFF
    if (!ParseInfo.IsDetected)
    {
        tiff TIFF(&Global.Errors);
        TIFF.FrameRate = ParseInfo.FrameRate.empty() ? &ParseInfo.FrameRate : NULL;
        if (ParseInfo.ParseFile_Input(TIFF, Input, Files_Pos))
            return 1;

        if (ParseInfo.IsDetected)
        {
            stringstream t;
            t << TIFF.slice_x * TIFF.slice_y;
            ParseInfo.Slices = t.str();
        }
    }

    // End
    if (Global.HasAtLeastOneFile && !Global.AcceptFiles)
    {
        Global.ProgressIndicator_Stop();
        cerr << "Input is a file so directory will not be handled as a whole.\nConfirm that this is what you want to do by adding \" --file\" to the command." << endl;
        return 1;
    }
    if (ParseInfo.IsDetected)
    {
        stream Stream;

        Stream.FileName = ParseInfo.RemovedFiles[0];
        if (!ParseInfo.FileName_StartNumber.empty() && !ParseInfo.FileName_Template.empty())
        {
            Stream.FileName_Template = ParseInfo.FileName_Template;
            Stream.FileName_StartNumber = ParseInfo.FileName_StartNumber;
            Stream.FileName_EndNumber = ParseInfo.FileName_EndNumber;
        }
        Stream.Flavor = ParseInfo.Flavor;
        Stream.Problem = ParseInfo.Problem;

        Stream.Slices = ParseInfo.Slices;
        if (!ParseInfo.FrameRate.empty())
            Stream.FrameRate = ParseInfo.FrameRate;

        Output.Streams.push_back(Stream);
    }

    return 0;
}

//---------------------------------------------------------------------------
int ParseFile_Compressed(parse_info& ParseInfo)
{
    // Init
    string OutputDirectoryName;
    if (Global.OutputFileName.empty() && ParseInfo.Name)
        OutputDirectoryName = *ParseInfo.Name + ".RAWcooked" + PathSeparator;
    else
    {
        OutputDirectoryName = Global.OutputFileName;
        if (!OutputDirectoryName.empty() && OutputDirectoryName.find_last_of("/\\") != OutputDirectoryName.size() - 1)
            OutputDirectoryName += PathSeparator;
    }

    // Matroska
    int ReturnValue = 0;
    bool NoOutputCheck = Global.Check && !Global.OutputFileName_IsProvided;
    if (!ParseInfo.IsDetected)
    {
        matroska M(OutputDirectoryName, &Global.Mode, Ask_Callback, &Global.Errors);
        M.Quiet = Global.Quiet;
        M.NoWrite = Global.Check;
        M.NoOutputCheck = NoOutputCheck;
        if (ParseInfo.ParseFile_Input(M))
        {
            ReturnValue = 1;
        }
    }

    // End
    if (ParseInfo.IsDetected && !Global.Quiet)
    {
        if (!Global.Check)
            cout << "\nFiles are in " << OutputDirectoryName << '.' << endl;
        else if (!Global.Errors.HasErrors())
            cout << '\n' << (NoOutputCheck ? "Decoding" : "Reversability") << " was checked, no issue detected." << endl;
    }
    if (Global.Check && Global.Errors.HasErrors())
        cout << '\n' << (NoOutputCheck ? "Decoding" : "Reversability") << " was checked, issues detected, see below." << endl;

    return ReturnValue;
}

//---------------------------------------------------------------------------
int ParseFile(size_t Files_Pos)
{
    // Init
    parse_info ParseInfo;
    ParseInfo.Name = &Input.Files[Files_Pos];

    // Open file
    if (int Value = ParseInfo.FileMap.Open_ReadMode(*ParseInfo.Name))
        return Value;

    // Compressed content
    if (int Value = ParseFile_Compressed(ParseInfo))
        return Value;
    if (ParseInfo.IsDetected)
        return 0;

    // Uncompressed content
    if (int Value = ParseFile_Uncompressed(ParseInfo, Files_Pos))
        return Value;
    if (ParseInfo.IsDetected)
        return 0;

    // Attachments
    size_t AttachmentSizeFinal = (Global.AttachmentMaxSize != (size_t)-1) ? Global.AttachmentMaxSize : (1024 * 1024); // Default value arbitrary choosen
    if (ParseInfo.FileMap.Buffer_Size >= AttachmentSizeFinal)
    {
        cout << "Error: " << *ParseInfo.Name << " is not small, expected to be an attachment?\nPlease contact info@mediaarea.net if you want support of such file." << endl;
        return 1;
    }
    if (ParseInfo.FileMap.Buffer_Size) // Ignoring file with size of 0
    {
        attachment Attachment;
        Attachment.FileName_In = *ParseInfo.Name;
        Attachment.FileName_Out = ParseInfo.Name->substr(Global.Path_Pos_Global);
        Output.Attachments.push_back(Attachment);
    }
    return 0;
}

//---------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
    // Manage command line
    if (int Value = Global.ManageCommandLine(argv, argc))
        return Value;

    // Analyze input
    if (int Value = Input.AnalyzeInputs(Global))
        return Value;
    sort(Input.Files.begin(), Input.Files.end());

    // Parse files
    RAWcooked.FileName = Global.rawcooked_reversibility_data_FileName;
    int Value = 0;
    for (int i = 0; i < Input.Files.size(); i++)
        if (Value = ParseFile(i))
            break;
    RAWcooked.Close();

    // Progress indicator
    Global.ProgressIndicator_Stop();

    // FFmpeg
    if (!Value)
        Value = Output.Process(Global);

    // RAWcooked file
    if (!Global.DisplayCommand)
        RAWcooked.Delete();

    // Global errors
    if (Global.Errors.ErrorMessage())
        cerr << Global.Errors.ErrorMessage() << endl;

    // Check result
    if (Global.Check && !Global.Errors.HasErrors() && !Global.OutputFileName.empty() && !Output.Streams.empty())
    {
        parse_info ParseInfo;
        Value = ParseInfo.FileMap.Open_ReadMode(Global.OutputFileName);
        if (!Value)
        {
            // Configure for a 2nd pass
            ParseInfo.Name = NULL;
            Global.OutputFileName = Global.Inputs[0];
            Global.OutputFileName_IsProvided = true;

            // Remove directory name (already in RAWcooked file data)
            size_t Path_Pos = Global.OutputFileName.find_last_of("/\\");
            if (Path_Pos == (size_t)-1)
                Path_Pos = 0;
            Global.OutputFileName.resize(Path_Pos);

            // Parse (check mode)
            Value = ParseFile_Compressed(ParseInfo);
        }

        // Global errors
        if (Global.Errors.ErrorMessage())
            cerr << Global.Errors.ErrorMessage() << endl;
    }

    return Value;
}
