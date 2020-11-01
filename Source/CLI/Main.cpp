/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
*
*  Use of this source code is governed by a BSD-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#include "CLI/Global.h"
#include "CLI/Input.h"
#include "CLI/Output.h"
#include "Lib/Compressed/Matroska/Matroska.h"
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include "Lib/Uncompressed/WAV/WAV.h"
#include "Lib/Uncompressed/AIFF/AIFF.h"
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
#include "Lib/ThirdParty/alphanum/alphanum.hpp"
#include "Lib/ThirdParty/thread-pool/include/ThreadPool.h"
#include <map>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <algorithm>
#include <thread>
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
struct parse_info
{
    string* Name;
    filemap FileMap;
    vector<string> RemovedFiles;
    string FileName_Template;
    string FileName_StartNumber;
    string FileName_EndNumber;
    string FileList;
    string Flavor;
    string Slices;
    input_info InputInfo;
    bool   IsDetected;
    bool   Problem;

    bool ParseFile_Input(input_base& Input, bool OverrideCheckPadding = false);
    bool ParseFile_Input(input_base_uncompressed& SingleFile, input& Input, size_t Files_Pos);

    parse_info():
        IsDetected(false),
        Problem(false)
    {}
};

//---------------------------------------------------------------------------
bool parse_info::ParseFile_Input(input_base& SingleFile, bool OverrideCheckPadding)
{
    // Init
    SingleFile.Actions = Global.Actions;
    if (OverrideCheckPadding)
        SingleFile.Actions.set(Action_CheckPadding);
    SingleFile.Hashes = &Global.Hashes;
    SingleFile.FileName = &RAWcooked.OutputFileName;
    SingleFile.InputInfo = &InputInfo;

    // Parse
    SingleFile.Parse(FileMap);
    Global.ProgressIndicator_Increment();

    // Management
    if (SingleFile.IsDetected() && SingleFile.ParserCode != Parser_Unknown && SingleFile.ParserCode != Parser_HashSum)
        IsDetected = true;

    if (Global.Errors.HasErrors())
    {
        if (strstr(Global.Errors.ErrorMessage(), "becoming too big"))
        {
            Global.ProgressIndicator_Stop();
            cerr << "Error: the reversibility file is becoming unexpectedly big.\n"
                    "       FFmpeg, used for muxing the output, has some issues with big\n"
                    "       attachments, and such big reversibility file is not expected\n"
                    "       with such compression, we prefer to be safe and we reject the\n"
                    "       compression.\n"
                    "       Please contact info@mediaarea.net if you want support of such input." << endl;
        }
        return true;
    }
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
    RAWcooked.ProgressIndicator_IsEnd = &Global.ProgressIndicator_IsEnd;
    RAWcooked.ProgressIndicator_IsPaused = &Global.ProgressIndicator_IsPaused;
    RAWcooked.Errors = &Global.Errors;
    SingleFile.RAWcooked = &RAWcooked;
    RAWcooked.OutputFileName = Name->substr(Global.Path_Pos_Global);
    FormatPath(RAWcooked.OutputFileName);

    // Parse
    if (ParseFile_Input((input_base&)SingleFile, !Global.Actions[Action_CheckPadding]))
        return true;
    if (!IsDetected)
        return false;

    // Management
    Flavor = SingleFile.Flavor_String();
    if (SingleFile.IsSequence)
        Input.DetectSequence(Global.HasAtLeastOneFile, Files_Pos, RemovedFiles, Global.Path_Pos_Global, FileName_Template, FileName_StartNumber, FileName_EndNumber, FileList, Global.Actions, &Global.Errors);
    if (RemovedFiles.empty())
        RemovedFiles.push_back(*Name);
    else
    {
        // OverrideCheckPadding info
        bool OverrideCheckPadding = !Global.Actions[Action_CheckPadding] && SingleFile.RAWcooked && SingleFile.RAWcooked->InData;
        if (OverrideCheckPadding) // There are non-zero padding bits
        {
            Global.ProgressIndicator_Stop();
            cerr << "Info: non-zero padding bits found in first file,\n"
                 << "      forcing the check of padding bits for all files.\n" << endl;
        }
        else if (SingleFile.MayHavePaddingBits() && !Global.Actions[Action_CheckPaddingOptionIsSet]) // If --no-checking-padding is not present
        {
            Global.ProgressIndicator_Stop();
            if (Global.Check)
            {
                cerr << "Info: data can contain non-zero padding bits, padding bits will be\n"
                        "      checked only during reversibility check so after encoding,\n"
                        "      and will throw an error if non-zero padding bits are found\n"
                        "      at this moment, and in that case you'll have to re-encode with\n"
                        "      --check-padding option.\n" << endl;
            }
            else
            {
                cerr << "Error: data may contain non-zero padding bits but padding would never\n"
                        "       be tested with the current options, there is a risk of non-\n"
                        "       reversibility, use --no-check-padding for explicitely indicate\n"
                        "       that you are fine with that else use --check-padding for checking\n"
                        "       the padding bits before encoding, or alternatively use --check for\n"
                        "       checking padding bits after encoding (you'll have to re-encode\n"
                        "       with --check-padding option if non-zero padding bits are found).\n" << endl;
                return true;
            }
        }

        Global.ProgressIndicator_Start(Input.Files.size() + RemovedFiles.size() - 1);
        SingleFile.InputInfo->FrameCount = RemovedFiles.size();
        for (size_t i = 1; i < SingleFile.InputInfo->FrameCount; i++)
        {
            Name = &RemovedFiles[i];
            if (input::OpenInput(FileMap, *Name, &Global.Errors))
                return true;
            RAWcooked.OutputFileName = Name->substr(Global.Path_Pos_Global);
            FormatPath(RAWcooked.OutputFileName);

            if (ParseFile_Input((input_base&)SingleFile, OverrideCheckPadding))
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
        if (ParseInfo.ParseFile_Input(TIFF, Input, Files_Pos))
            return 1;

        if (ParseInfo.IsDetected)
        {
            stringstream t;
            t << TIFF.slice_x * TIFF.slice_y;
            ParseInfo.Slices = t.str();
        }
    }

    // Unknown (but test if it is hashsum first)
    if (!ParseInfo.IsDetected)
    {
        bool HashFileParsed;
        if (Global.Actions[Action_Conch])
        {
            hashsum HashSum;
            HashSum.HomePath = ParseInfo.Name->substr(Global.Path_Pos_Global);
            HashSum.List = &Global.Hashes;
            if (ParseInfo.ParseFile_Input(HashSum, Input, Files_Pos))
                return 1;
            HashFileParsed = HashSum.IsDetected();
        }
        else
            HashFileParsed = false;
        if (HashFileParsed)
            Global.Hashes.Ignore(RAWcooked.OutputFileName);
        else
        {
            unknown Unknown;
            if (ParseInfo.ParseFile_Input(Unknown, Input, Files_Pos))
                return 1;
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
            Stream.FileList = ParseInfo.FileList;
        }
        Stream.Flavor = ParseInfo.Flavor;
        Stream.Problem = ParseInfo.Problem;

        Stream.Slices = ParseInfo.Slices;
        map<string, string>::iterator FrameRateFromOptions = Global.VideoInputOptions.find("framerate");
        if (FrameRateFromOptions != Global.VideoInputOptions.end())
        {
            Stream.FrameRate = FrameRateFromOptions->second;
            ParseInfo.InputInfo.FrameRate = stod(Stream.FrameRate);
        }
        else if (ParseInfo.InputInfo.FrameRate)
            Stream.FrameRate = to_string(ParseInfo.InputInfo.FrameRate);

        Output.Streams.push_back(Stream);

        if (Global.Actions[Action_Coherency])
        {
            // Duration
            double Duration;
            if (ParseInfo.InputInfo.SampleCount && ParseInfo.InputInfo.SampleRate)
                Duration = ParseInfo.InputInfo.SampleCount / ParseInfo.InputInfo.SampleRate;
            else if (ParseInfo.InputInfo.FrameCount && ParseInfo.InputInfo.FrameRate)
                Duration = ParseInfo.InputInfo.FrameCount / ParseInfo.InputInfo.FrameRate;
            else
                Duration = 0;
            if (Duration)
            {
                Global.Durations.push_back(Duration);
                Global.Durations_FileName.push_back(RAWcooked.OutputFileName);
            }
        }
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
        // Threads
        unsigned threads;
        auto OutputOptions_Threads = Global.OutputOptions.find("threads");
        if (OutputOptions_Threads != Global.OutputOptions.end())
            threads = stoul(OutputOptions_Threads->second);
        else
            threads = 0;
        if (!threads)
            threads = thread::hardware_concurrency();
        ThreadPool* Thread_Pool;
        if (threads > 1)
        {
            Thread_Pool = new ThreadPool(threads);
            Thread_Pool->init();
        }
        else
            Thread_Pool = nullptr;

        matroska* M = new matroska(OutputDirectoryName, &Global.Mode, Ask_Callback, Thread_Pool, &Global.Errors);
        M->Quiet = Global.Quiet;
        M->NoWrite = Global.Check;
        M->NoOutputCheck = NoOutputCheck;
        if (NoOutputCheck)
            NoOutputCheck = M->Hashes_FromRAWcooked ? false : true; // If hashes are present in the file, output was checked by using hashes
        if (ParseInfo.ParseFile_Input(*M))
        {
            ReturnValue = 1;
        }
        delete M;
        delete Thread_Pool;
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
    if (input::OpenInput(ParseInfo.FileMap, *ParseInfo.Name, &Global.Errors))
        return 1;

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
    if (ParseInfo.FileMap.Size() >= AttachmentSizeFinal)
    {
        cout << "Error: " << *ParseInfo.Name << " is not small, expected to be an attachment?\nPlease contact info@mediaarea.net if you want support of such file." << endl;
        return 1;
    }
    if (!ParseInfo.FileMap.Empty()) // Ignoring file with size of 0
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
    // Global configuration
    Global.Ask_Callback = Ask_Callback;
    
    // Manage command line
    if (int Value = Global.ManageCommandLine(argv, argc))
        return Value;

    // Analyze input
    if (int Value = Input.AnalyzeInputs(Global))
        return Value;
    sort(Input.Files.begin(), Input.Files.end(),
        [](const string& l, const string& r) {return doj::alphanum_comp(l, r) < 0; });

    // Parse files
    RAWcooked.FileName = Global.rawcooked_reversibility_FileName;
    int Value = 0;
    for (size_t i = 0; i < Input.Files.size(); i++)
    {
        Value = ParseFile(i);
        if (Value)
            break;
    }
    RAWcooked.Close();

    // Coherency checks
    if (Global.Actions[Action_Coherency])
    {
        input::CheckDurations(Global.Durations, Global.Durations_FileName, &Global.Errors);
    }

    // Hashes
    if (Global.Actions[Action_Hash])
    {
        Global.Hashes.NoMoreHashFiles();
        Global.Hashes.Finish();
        Global.Hashes.CheckFromFiles = true;
        Global.Hashes.WouldBeError = true;
    }

    // Progress indicator
    Global.ProgressIndicator_Stop();

    if (!Value && Global.Errors.HasWarnings())
    {
        cerr << Global.Errors.ErrorMessage() << endl;
        switch (Global.Mode)
        {
            case AlwaysNo: Value = 1; break;
            case AlwaysYes: break;
            default:
                if ((!Value && Global.Actions[Action_Encode] && !Output.Streams.empty())
                 || (Global.Check && !Global.Errors.HasErrors() && !Global.OutputFileName.empty() && !Output.Streams.empty()))
                {
                    cerr << "Do you want to continue despite warnings? [y/N] ";
                    string Result;
                    getline(cin, Result);
                    if (!(!Result.empty() && (Result[0] == 'Y' || Result[0] == 'y')))
                        Value = 1;
                }
        }
    }

    // FFmpeg
    if (!Value && Global.Actions[Action_Encode])
        Value = Output.Process(Global);

    // RAWcooked file
    if (!Global.DisplayCommand)
        RAWcooked.Delete();

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
            if (!Global.Actions[Action_Hash])
                Global.OutputFileName_IsProvided = true;
            RAWcooked.OutputFileName.clear();

            // Remove directory name (already in RAWcooked file data)
            size_t Path_Pos = Global.OutputFileName.find_last_of("/\\");
            if (Path_Pos == Global.OutputFileName.size() - 1 && Global.OutputFileName.size() > 1)
                Path_Pos = Global.OutputFileName.find_last_of("/\\", Global.OutputFileName.size() - 2); // Ignore the trailing path separator
            if (Path_Pos == (size_t)-1)
                Path_Pos = 0;
            Global.OutputFileName.resize(Path_Pos);

            // Parse (check mode)
            Value = ParseFile_Compressed(ParseInfo);
        }
    }

    // Global errors
    if (Global.Errors.HasErrors())
    {
        cerr << Global.Errors.ErrorMessage() << endl;
        if (!Value)
            Value = 1;
    }

    return Value;
}
