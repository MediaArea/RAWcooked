/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/Global.h"
#include "CLI/Help.h"
#include <iostream>
#include <cstring>
#include <iomanip>
#include <thread>
#if defined(_WIN32) || defined(_WINDOWS)
    #include <direct.h>
    #define getcwd _getcwd
#else
    #include <unistd.h>
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Glue
void global_ProgressIndicator_Show(global* G)
{
    G->ProgressIndicator_Show();
}

//---------------------------------------------------------------------------
int global::SetOutputFileName(const char* FileName)
{
    OutputFileName = FileName;
    OutputFileName_IsProvided = true;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetBinName(const char* FileName)
{
    BinName = FileName;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetLicenseKey(const char* Key, bool StoreIt)
{
    LicenseKey = Key;
    StoreLicenseKey = StoreIt;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetSubLicenseId(uint64_t Id)
{
    if (!Id || Id >= 127)
    {
        cerr << "Error: sub-licensee ID must be between 1 and 126.\n";
        return 1;
    }
    SubLicenseId = Id;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetSubLicenseDur(uint64_t Dur)
{
    if (Dur > 12)
    {
        cerr << "Error: sub-licensee duration must be between 0 and 12.\n";
        return 1;
    }
    SubLicenseDur = Dur;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetDisplayCommand()
{
    DisplayCommand = true;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetAcceptFiles()
{
    AcceptFiles = true;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetCheck(bool Value)
{
    Actions.set(Action_Check, Value);
    Actions.set(Action_CheckOptionIsSet);
    if (Value)
        return SetDecode(false);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetQuickCheck()
{
    Actions.set(Action_Check, false);
    Actions.set(Action_CheckOptionIsSet, false);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetCheck(const char* Value, int& i)
{
    if (Value && (strcmp(Value, "0") == 0 || strcmp(Value, "partial") == 0))
    {
        SetCheckPadding(false);
        ++i; // Next argument is used
        cerr << "Warning: \" --check " << Value << "\" is deprecated, use \" --no-check-padding\" instead.\n" << endl;
        return 0;
    }
    if (Value && (strcmp(Value, "1") == 0 || strcmp(Value, "full") == 0))
    {
        SetCheckPadding(true);
        ++i; // Next argument is used
        cerr << "Warning: \" --check " << Value << "\" is deprecated, use \" --check-padding\" instead.\n" << endl;
        return 0;
    }

    SetCheck(true);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetCheckPadding(bool Value)
{
    Actions.set(Action_CheckPadding, Value);
    Actions.set(Action_CheckPaddingOptionIsSet);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetQuickCheckPadding()
{
    Actions.set(Action_CheckPadding, false);
    Actions.set(Action_CheckPaddingOptionIsSet, false);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetAcceptGaps(bool Value)
{
    Actions.set(Action_AcceptGaps, Value);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetVersion(const char* Value)
{
    if (Value[0] == '1' && !Value[1])
    {
        Actions.set(Action_VersionValueIsAuto, false);
        Actions.set(Action_Version2, false);
        return 0;
    }
    if (Value[0] == '2' && !Value[1])
    {
        Actions.set(Action_VersionValueIsAuto, false);
        Actions.set(Action_Version2);
        return 0;
    }
    cerr << "Error: unknown version value '" << Value << "'." << endl;
    return 1;
}

//---------------------------------------------------------------------------
int global::SetCoherency(bool Value)
{
    Actions.set(Action_Coherency, Value);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetConch(bool Value)
{
    Actions.set(Action_Conch, Value);
    if (Value)
    {
        SetDecode(false);
        SetEncode(false);
    }
    return 0;
}

//---------------------------------------------------------------------------
int global::SetDecode(bool Value)
{
    Actions.set(Action_Decode, Value);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetEncode(bool Value)
{
    Actions.set(Action_Encode, Value);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetInfo(bool Value)
{
    Actions.set(Action_Info, Value);
    if (Value)
    {
        SetDecode(false);
        SetEncode(false);
    }
    return 0;
}

//---------------------------------------------------------------------------
int global::SetFrameMd5(bool Value)
{
    Actions.set(Action_FrameMd5, Value);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetFrameMd5An(bool Value)
{
    Actions.set(Action_FrameMd5An, Value);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetFrameMd5FileName(const char* FileName)
{
    FrameMd5FileName = FileName;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetHash(bool Value)
{
    Actions.set(Action_Hash, Value);
    return 0;
}

//---------------------------------------------------------------------------
int global::SetFileOpenMethod(const char* Value)
{
    if (strcmp(Value, "mmap") == 0)
    {
        FileOpenMethod = filemap::method::mmap;
        return 0;
    }
    if (strcmp(Value, "fstream") == 0)
    {
        FileOpenMethod = filemap::method::fstream;
        return 0;
    }
    if (strcmp(Value, "fopen") == 0)
    {
        FileOpenMethod = filemap::method::fopen;
        return 0;
    }
    if (strcmp(Value, "open") == 0)
    {
        FileOpenMethod = filemap::method::open;
        return 0;
    }
    #if defined(_WIN32) || defined(_WINDOWS)
    if (strcmp(Value, "createfile") == 0)
    {
        FileOpenMethod = filemap::method::createfile;
        return 0;
    }
    #endif //defined(_WIN32) || defined(_WINDOWS)
    cerr << "Error: unknown io value '" << Value << "'." << endl;
    return 1;
}

//---------------------------------------------------------------------------
int global::SetAll(bool Value)
{
    if (int ReturnValue = SetInfo(Value))
        return ReturnValue;
    if (int ReturnValue = SetConch(Value))
        return ReturnValue;
    if (int ReturnValue = (Value?SetCheck(true):SetQuickCheck())) // Never implicitely set no check
        return ReturnValue;
    if (int ReturnValue = (Value && SetCheckPadding(Value))) // Never implicitely set no check padding
        return ReturnValue;
    if (int ReturnValue = SetAcceptGaps(Value))
        return ReturnValue;
    if (int ReturnValue = SetCoherency(Value))
        return ReturnValue;
    if (int ReturnValue = SetDecode(Value))
        return ReturnValue;
    if (int ReturnValue = SetEncode(Value))
        return ReturnValue;
    if (int ReturnValue = SetHash(Value))
        return ReturnValue;
    return 0;
}

//---------------------------------------------------------------------------
int Error_NotTested(const char* Option1, const char* Option2 = NULL)
{
    cerr << "Error: option " << Option1;
    if (Option2)
        cerr << ' ' << Option2;
    cerr << " not yet tested.\nPlease contact info@mediaarea.net if you want support of such option." << endl;
    return 1;
}

//---------------------------------------------------------------------------
int Error_Missing(const char* Option1)
{
    cerr << "Error: missing argument for option '" << Option1 << "'." << endl;
    return 1;
}

//---------------------------------------------------------------------------
int global::SetOption(const char* argv[], int& i, int argc)
{
    if (strcmp(argv[i], "-c:a") == 0)
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (strcmp(argv[i], "copy") == 0)
        {
            OutputOptions["c:a"] = argv[i];
            License.Encoder(encoder::PCM);
            return 0;
        }
        if (strcmp(argv[i], "flac") == 0)
        {
            OutputOptions["c:a"] = argv[i];
            License.Encoder(encoder::FLAC);
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-c:v"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "ffv1"))
        {
            OutputOptions["c:v"] = argv[i];
            License.Encoder(encoder::FFV1);
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-coder"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "0")
         || !strcmp(argv[i], "1")
         || !strcmp(argv[i], "2"))
        {
            OutputOptions["coder"] = argv[i];
            License.Feature(feature::EncodingOptions);
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-context"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "0")
         || !strcmp(argv[i], "1"))
        {
            OutputOptions["context"] = argv[i];
            License.Feature(feature::EncodingOptions);
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-f"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "matroska"))
        {
            OutputOptions["f"] = argv[i];
            return 0;
        }
        return 0;
    }
    if (!strcmp(argv[i], "-g"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (atoi(argv[i]))
        {
            OutputOptions["g"] = argv[i];
            License.Feature(feature::EncodingOptions);
            return 0;
        }
        cerr << "Invalid \"" << argv[i - 1] << " " << argv[i] << "\" value, it must be a number\n";
        return 1;
    }
    if (!strcmp(argv[i], "-level"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "0")
         || !strcmp(argv[i], "1")
         || !strcmp(argv[i], "3"))
        {
            OutputOptions["level"] = argv[i];
            License.Feature(feature::EncodingOptions);
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-map"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "0"))
        {
            // Do nothing
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-slicecrc"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "0")
         || !strcmp(argv[i], "1"))
        {
            OutputOptions["slicecrc"] = argv[i];
            License.Feature(feature::EncodingOptions);
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-slices"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        int SliceCount = atoi(argv[i]);
        if (SliceCount) //TODO: not all slice counts are accepted by FFmpeg, we should filter
        {
            OutputOptions["slices"] = argv[i];
            License.Feature(feature::EncodingOptions);
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }

    return Error_NotTested(argv[i]);
}

//---------------------------------------------------------------------------
int global::ManageCommandLine(const char* argv[], int argc)
{
    if (argc < 2)
        return Usage(argv[0]);

    AttachmentMaxSize = (size_t)-1;
    IgnoreLicenseKey = !License.IsSupported_License();
    SubLicenseId = 0;
    SubLicenseDur = 1;
    FileOpenMethod = filemap::method::mmap;
    ShowLicenseKey = false;
    StoreLicenseKey = false;
    DisplayCommand = false;
    AcceptFiles = false;
    OutputFileName_IsProvided = false;
    Quiet = false;
    Actions.set(Action_Encode);
    Actions.set(Action_Decode);
    Actions.set(Action_Coherency);
    Hashes = hashes(&Errors);
    ProgressIndicator_Thread = NULL;

    for (int i = 1; i < argc; i++)
    {
        if ((argv[i][0] == '.' && argv[i][1] == '\0')
         || (argv[i][0] == '.' && (argv[i][1] == '/' || argv[i][1] == '\\') && argv[i][2] == '\0'))
        {
            //translate to "../xxx" in order to get the top level directory name
            char buff[FILENAME_MAX];
            if (getcwd(buff, FILENAME_MAX))
            {
                string Arg = buff;
                size_t Path_Pos = Arg.find_last_of("/\\");
                Arg = ".." + Arg.substr(Path_Pos);
                Inputs.push_back(Arg);
            }
            else
            {
                cerr << "Error: " << argv[i] << " can not be transformed to a directory name." << endl;
                return 1;
            }
        }
        else if (strcmp(argv[i], "--all") == 0)
        {
            int Value = SetAll(true);
            if (Value)
                return Value;
        }
        else if ((strcmp(argv[i], "--attachment-max-size") == 0 || strcmp(argv[i], "-s") == 0))
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            AttachmentMaxSize = atoi(argv[++i]);
            License.Feature(feature::GeneralOptions);
        }
        else if ((strcmp(argv[i], "--bin-name") == 0 || strcmp(argv[i], "-b") == 0))
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            int Value = SetBinName(argv[++i]);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--check") == 0)
        {
            if (i + 1 < argc)
            {
                // Deprecated version handling
                int Value = SetCheck(argv[i + 1], i);
                if (Value)
                    return Value;
            }
            else
            {
                int Value = SetCheck(true);
                if (Value)
                    return Value;
            }
        }
        else if (strcmp(argv[i], "--check-padding") == 0)
        {
            int Value = SetCheckPadding(true);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--accept-gaps") == 0)
        {
            int Value = SetAcceptGaps(true);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--coherency") == 0)
        {
            int Value = SetCoherency(true);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--conch") == 0)
        {
            int Value = SetConch(true);
            if (Value)
                return Value;
        }
        else if ((strcmp(argv[i], "--display-command") == 0 || strcmp(argv[i], "-d") == 0))
        {
            int Value = SetDisplayCommand();
            if (Value)
                return Value;
            License.Feature(feature::GeneralOptions);
        }
        else if (strcmp(argv[i], "--decode") == 0)
        {
            int Value = SetDecode(true);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--encode") == 0)
        {
            int Value = SetEncode(true);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--framemd5") == 0)
        {
            int Value = SetFrameMd5(true);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--framemd5-an") == 0)
        {
            int Value = SetFrameMd5An(true);
            if (Value)
                return Value;
            Value = SetFrameMd5(true);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--framemd5-name") == 0)
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            int Value = SetFrameMd5FileName(argv[++i]);
            if (Value)
                return Value;
            Value = SetFrameMd5(true);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--hash") == 0)
        {
            int Value = SetHash(true);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--file") == 0)
        {
            int Value = SetAcceptFiles();
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            int Value = Help(argv[0]);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--info") == 0)
        {
            int Value = SetInfo(true);
            if (Value)
                return Value;
        }
        else if ((strcmp(argv[i], "--license") == 0 || strcmp(argv[i], "--licence") == 0))
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            int Value = SetLicenseKey(argv[++i], false);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--no-accept-gaps") == 0)
        {
            int Value = SetAcceptGaps(false);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--no-check") == 0)
        {
            int Value = SetCheck(false);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--no-check-padding") == 0)
        {
            int Value = SetCheckPadding(false);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--no-coherency") == 0)
        {
        int Value = SetCoherency(false);
        if (Value)
            return Value;
        }
        else if (strcmp(argv[i], "--no-conch") == 0)
        {
            int Value = SetConch(false);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--no-decode") == 0)
        {
            int Value = SetDecode(false);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--no-encode") == 0)
        {
            int Value = SetEncode(false);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--no-hash") == 0)
        {
            int Value = SetHash(false);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--no-info") == 0)
        {
            int Value = SetInfo(false);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--none") == 0)
        {
            int Value = SetAll(false);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--quick-check") == 0)
        {
            int Value = SetQuickCheck();
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--quick-check-padding") == 0)
        {
            int Value = SetQuickCheckPadding();
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--version") == 0)
        {
            int Value = Version();
            if (Value)
                return Value;
        }
        else if ((strcmp(argv[i], "--output-name") == 0 || strcmp(argv[i], "-o") == 0))
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            int Value = SetOutputFileName(argv[++i]);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--output-version") == 0)
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            if (int Value = SetVersion(argv[++i]))
                return Value;
        }
        else if (strcmp(argv[i], "--quiet") == 0)
        {
            OutputOptions["loglevel"] = "warning";
            Quiet = true;
        }
        else if ((strcmp(argv[i], "--rawcooked-file-name") == 0 || strcmp(argv[i], "-r") == 0))
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            rawcooked_reversibility_FileName = argv[++i];
        }
        else if (strcmp(argv[i], "--show-license") == 0 || strcmp(argv[i], "--show-licence") == 0)
        {
            ShowLicenseKey = true;
        }
        else if (strcmp(argv[i], "--sublicense") == 0 || strcmp(argv[i], "--sublicence") == 0)
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            License.Feature(feature::SubLicense);
            if (auto Value = SetSubLicenseId(atoi(argv[++i])))
                return Value;
        }
        else if (strcmp(argv[i], "--sublicense-dur") == 0 || strcmp(argv[i], "--sublicence-dur") == 0)
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            License.Feature(feature::SubLicense);
            if (auto Value = SetSubLicenseDur(atoi(argv[++i])))
                return Value;
        }
        else if ((strcmp(argv[i], "--store-license") == 0 || strcmp(argv[i], "--store-licence") == 0))
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            int Value = SetLicenseKey(argv[++i], true);
            if (Value)
                return Value;
        }
        else if (!strcmp(argv[i], "-i"))
        {
            if (OptionsForOtherFiles)
            {
                cerr << "-i after first output file name not tested\n";
                return 1;
            }
            if (++i >= argc)
                return Error_NotTested(argv[i - 1]);
            Inputs.push_back(argv[i]);
            NextFileNameIsOutput = true;
            if (auto Value = SetAcceptFiles())
                return Value;
        }
        else if (strcmp(argv[i], "--io") == 0)
        {
            if (i + 1 == argc)
                return Error_Missing(argv[i]);
            int Value = SetFileOpenMethod(argv[++i]);
            if (Value)
                return Value;
        }
        else if (!strcmp(argv[i], "-framerate"))
        {
            if (OptionsForOtherFiles)
            {
                cerr << "-framerate after first output file name not tested\n";
                return 1;
            }
            if (++i >= argc)
                return Error_NotTested(argv[i - 1]);
            if (!atof(argv[i]))
            {
                cerr << "Invalid \"" << argv[i - 1] << " " << argv[i] << "\" value, it must be a number\n";
                return 1;
            }
            VideoInputOptions["framerate"] = argv[i];
            License.Feature(feature::InputOptions);
        }
        else if (!strcmp(argv[i], "-loglevel") || !strcmp(argv[i], "-v"))
        {
            if (++i >= argc)
                return Error_NotTested(argv[i - 1]);
            if (strcmp(argv[i], "error")
             && strcmp(argv[i], "warning"))
                return Error_NotTested(argv[i - 1], argv[i]);
            OutputOptions["loglevel"] = argv[i];
            Quiet = true;
        }
        else if (strcmp(argv[i], "-n") == 0)
        {
            OutputOptions["n"] = string();
            OutputOptions.erase("y");
            Mode = AlwaysNo; // Also RAWcooked itself
        }
        else if (!strcmp(argv[i], "-threads"))
        {
            if (++i >= argc)
                return Error_NotTested(argv[i - 1]);
            OutputOptions["threads"] = argv[i];
            License.Feature(feature::GeneralOptions);
        }
        else if (strcmp(argv[i], "-y") == 0)
        {
            OutputOptions["y"] = string();
            OutputOptions.erase("n");
            Mode = AlwaysYes; // Also RAWcooked itself
        }
        else if (OptionsForOtherFiles)
        {
            MoreOutputOptions.push_back(argv[i]);
        }
        else if (argv[i][0] == '-' && argv[i][1] != '\0')
        {
            int Value = SetOption(argv, i, argc);
            if (Value)
                return Value;
        }
        else if (NextFileNameIsOutput)
        {
            int Value = SetOutputFileName(argv[i]);
            if (Value)
                return Value;
            OptionsForOtherFiles = true;
            Value = SetAcceptFiles();
            if (Value)
                return Value;
        }
        else
            Inputs.push_back(argv[i]);
    }

    // License
    if (License.LoadLicense(LicenseKey, StoreLicenseKey))
        return true;
    if (!License.IsSupported())
    {
        cerr << "\nOne or more requested features are not supported with the current license key.\n";
        cerr << "****** Please contact info@mediaarea.net for a quote or a temporary key." << endl;
        if (!IgnoreLicenseKey)
            return 1;
        cerr << "       Ignoring the license for the moment.\n" << endl;
    }
    if (License.ShowLicense(ShowLicenseKey, SubLicenseId, SubLicenseDur))
        return 1;
    if (Inputs.empty() && (ShowLicenseKey || SubLicenseId))
        return 0;

    return 0;
}

//---------------------------------------------------------------------------
int global::SetDefaults()
{
    // Container format
    if (OutputOptions.find("f") == OutputOptions.end())
        OutputOptions["f"] = "matroska"; // Container format is Matroska

    // Video format
    if (OutputOptions.find("c:v") == OutputOptions.end())
        OutputOptions["c:v"] = "ffv1"; // Video format is FFV1

    // Audio format
    if (OutputOptions.find("c:a") == OutputOptions.end())
        OutputOptions["c:a"] = "flac"; // Audio format is FLAC
                                 
    // FFV1 specific
    if (OutputOptions["c:v"] == "ffv1")
    {
        if (OutputOptions.find("coder") == OutputOptions.end())
            OutputOptions["coder"] = "1"; // Range Coder
        if (OutputOptions.find("context") == OutputOptions.end())
            OutputOptions["context"] = "1"; // Small context
        if (OutputOptions.find("g") == OutputOptions.end())
            OutputOptions["g"] = "1"; // Intra
        if (OutputOptions.find("level") == OutputOptions.end())
        {
            auto slices = OutputOptions.find("slices");
            if (slices != OutputOptions.end() && slices->second == "1")
                OutputOptions["level"] = "1"; // FFV1 v1 when no slice
            else
                OutputOptions["level"] = "3"; // FFV1 v3
        }
        if (OutputOptions.find("slicecrc") == OutputOptions.end())
        {
            auto Level = OutputOptions.find("level");
            if (Level == OutputOptions.end() || (Level->second != "0" && Level->second != "1"))
                OutputOptions["slicecrc"] = "1"; // Slice CRC on
        }

        // Check incompatible options
        if (OutputOptions["level"] == "0" || OutputOptions["level"] == "1")
        {
            map<string, string>::iterator slices = OutputOptions.find("slices");
            if (slices == OutputOptions.end() || slices->second != "1")
            {
                cerr << "\" -level " << OutputOptions["level"] << "\" does not permit slices, is it intended ? if so, add \" -slices 1\" to the command." << endl;
                return 1;
            }
        }
    }

    return 0;
}

//---------------------------------------------------------------------------
void global::ProgressIndicator_Start(size_t Total)
{
    if (ProgressIndicator_Thread)
        return;
    ProgressIndicator_Current = 0;
    ProgressIndicator_Total = Total;
    ProgressIndicator_Thread = new thread(global_ProgressIndicator_Show, this);
}

//---------------------------------------------------------------------------
void global::ProgressIndicator_Stop()
{
    if (!ProgressIndicator_Thread)
        return;
    ProgressIndicator_Current = ProgressIndicator_Total;
    ProgressIndicator_IsEnd.notify_one();
    ProgressIndicator_Thread->join();
    delete ProgressIndicator_Thread;
    ProgressIndicator_Thread = NULL;
}

//---------------------------------------------------------------------------
// Progress indicator show
void global::ProgressIndicator_Show()
{
    // Configure progress indicator precision
    size_t ProgressIndicator_Value = (size_t)-1;
    size_t ProgressIndicator_Frequency = 100;
    streamsize Precision = 0;
    cerr.setf(ios::fixed, ios::floatfield);

    // Configure benches
    using namespace chrono;
    steady_clock::time_point Clock_Init = steady_clock::now();
    steady_clock::time_point Clock_Previous = Clock_Init;
    uint64_t FileCount_Previous = 0;

    // Show progress indicator at a specific frequency
    const chrono::seconds Frequency = chrono::seconds(1);
    size_t StallDetection = 0;
    mutex Mutex;
    unique_lock<mutex> Lock(Mutex);
    do
    {
        if (ProgressIndicator_IsPaused)
            continue;

        size_t ProgressIndicator_New = (size_t)(((float)ProgressIndicator_Current) * ProgressIndicator_Frequency / ProgressIndicator_Total);
        if (ProgressIndicator_New == ProgressIndicator_Value)
        {
            StallDetection++;
            if (StallDetection >= 4)
            {
                while (ProgressIndicator_New == ProgressIndicator_Value && ProgressIndicator_Frequency < 10000)
                {
                    ProgressIndicator_Frequency *= 10;
                    ProgressIndicator_Value *= 10;
                    Precision++;
                    ProgressIndicator_New = (size_t)(((float)ProgressIndicator_Current) * ProgressIndicator_Frequency / ProgressIndicator_Total);
                }
            }
        }
        else
            StallDetection = 0;
        if (ProgressIndicator_New != ProgressIndicator_Value)
        {
            float FileRate = 0;
            if (ProgressIndicator_Value != (size_t)-1)
            {
                steady_clock::time_point Clock_Current = steady_clock::now();
                steady_clock::duration Duration = Clock_Current - Clock_Previous;
                FileRate = (float)(ProgressIndicator_Current - FileCount_Previous) * 1000 / duration_cast<milliseconds>(Duration).count();
                Clock_Previous = Clock_Current;
                FileCount_Previous = ProgressIndicator_Current;
            }
            cerr << '\r';
            cerr << "Analyzing files (" << setprecision(Precision) << ((float)ProgressIndicator_New) * 100 / ProgressIndicator_Frequency << "%)";
            if (FileRate)
            {
                cerr << ", ";
                cerr << setprecision(0) << FileRate;
                cerr << " files/s";
            }
            cerr << "    "; // Clean up in case there is less content outputed than the previous time

            ProgressIndicator_Value = ProgressIndicator_New;
        }
    } while (ProgressIndicator_IsEnd.wait_for(Lock, Frequency) == cv_status::timeout && ProgressIndicator_Current != ProgressIndicator_Total);

    // Show summary
    cerr << '\r';
    cerr << "                                                            \r"; // Clean up in case there is less content outputed than the previous time
}
