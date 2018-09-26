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
#include "Lib/WAV/WAV.h"
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

    bool ParseFile_Input(input_base& Input);
    bool ParseFile_Input(input_base_uncompressed& SingleFile, input& Input, size_t Files_Pos);

    parse_info():
        IsDetected(false)
    {}
};

//---------------------------------------------------------------------------
bool parse_info::ParseFile_Input(input_base& SingleFile)
{
    // Init
    SingleFile.Buffer = FileMap.Buffer;
    SingleFile.Buffer_Size = FileMap.Buffer_Size;

    // Parse
    SingleFile.Parse(false, Global.FullCheck);
    Global.ProgressIndicator_Increment();
    if (SingleFile.ErrorMessage())
    {
        Global.ProgressIndicator_Stop();
        cerr << SingleFile.ErrorType_Before() << Name->substr(Global.Path_Pos_Global) << ' ' << SingleFile.ErrorMessage() << SingleFile.ErrorType_After() << endl;
        return true;
    }

    // Management
    if (SingleFile.IsDetected)
        IsDetected = true;

    return false;
}

//---------------------------------------------------------------------------
bool parse_info::ParseFile_Input(input_base_uncompressed& SingleFile, input& Input, size_t Files_Pos)
{
    if (IsDetected)
        return false;

    // Init
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
        Input.DetectSequence(Files_Pos, RemovedFiles, Global.Path_Pos_Global, FileName_Template, FileName_StartNumber, FileName_EndNumber);
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
        wav WAV;
        if (ParseInfo.ParseFile_Input(WAV, Input, Files_Pos))
            return 1;
    }

    // DPX
    if (!ParseInfo.IsDetected)
    {
        dpx DPX;
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

    // End
    if (Global.HasAtLeastOneFile && !Global.AcceptFiles)
    {
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

        Stream.Slices = ParseInfo.Slices;
        if (!ParseInfo.FrameRate.empty())
            Stream.FrameRate = ParseInfo.FrameRate;

        Output.Streams.push_back(Stream);
    }

    return 0;
}

//---------------------------------------------------------------------------
int ParseFile_Compressed(parse_info& ParseInfo, size_t Files_Pos)
{
    // Init
    string OutputDirectoryName;
    if (Global.OutputFileName.empty())
        OutputDirectoryName = *ParseInfo.Name + ".RAWcooked" + PathSeparator;
    else
    {
        OutputDirectoryName = Global.OutputFileName;
        if (OutputDirectoryName.find_last_of("/\\") != OutputDirectoryName.size() - 1)
            OutputDirectoryName += PathSeparator;
    }

    // Matroska
    if (!ParseInfo.IsDetected)
    {
        matroska M;
        M.OutputDirectoryName = OutputDirectoryName;
        M.Quiet = Global.Quiet;
        if (ParseInfo.ParseFile_Input(M))
            return 1;
    }

    // End
    if (ParseInfo.IsDetected && !Global.Quiet)
        cout << "Files are in " << OutputDirectoryName << '.' << endl;
    return 0;
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
    if (int Value = ParseFile_Compressed(ParseInfo, Files_Pos))
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
    for (int i = 0; i < Input.Files.size(); i++)
        if (int Value = ParseFile(i))
            return Value;
    RAWcooked.Close();

    // Progress indicator
    Global.ProgressIndicator_Stop();

    // FFmpeg
    if (int Value = Output.Process(Global))
        return Value;

    return 0;
}
