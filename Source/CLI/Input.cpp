/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/Input.h"
#include "Lib/Config.h"
#include "Lib/Input_Base.h"
#include <iostream>
#include <algorithm>
#if defined(_WIN32) || defined(_WINDOWS)
    #include "windows.h"
    #include <io.h> // File existence
    #define access _access_s
    #define stat _stat
    #define	S_ISDIR(m)	((m & 0170000) == 0040000)
#else
    #include <dirent.h>
    #include <fcntl.h>
    #include <glob.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/mman.h>
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace fileinput_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "file (can not be open)",
};

enum code : uint8_t
{
    FileCanNotBeOpen,
    Max
};

static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage);

} // undecodable

namespace incoherent
{

static const char* MessageText[] =
{
    "file names (gap in file names)",
    "A/V sync (durations are different)",
};

enum code : uint8_t
{
    FileMissing,
    Duration,
    Max
};

namespace unsupported { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // incoherent

const char** ErrorTexts[] =
{
    undecodable::MessageText,
    nullptr,
    incoherent::MessageText,
    nullptr,
};

static_assert(error::Type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // fileinput_issue

//---------------------------------------------------------------------------
bool IsDir(const char* Name)
{
    struct stat buffer;
    int status = stat(Name, &buffer);
    return (status == 0 && S_ISDIR(buffer.st_mode));
}

//---------------------------------------------------------------------------
void DetectPathPos(const string &Name, size_t& Path_Pos)
{
    string FN(Name);
    string Path;
    string After;
    string Before;
    Path_Pos = FN.find_last_of("/\\");
    if (Path_Pos != (size_t)-1)
    {
        Path_Pos++;
        Path = FN.substr(0, Path_Pos);
        if (Path_Pos > 2)
        {
            size_t Path_Pos2 = FN.find_last_of("/\\", Path_Pos - 2);
            FN.erase(0, Path_Pos);

            if (Path_Pos2 != (size_t)-1)
                Path_Pos = Path_Pos2 + 1;
            else
                Path_Pos = 0;
        }
    }
    else
        Path_Pos = 0;
}

//---------------------------------------------------------------------------
void input::DetectSequence(bool CheckIfFilesExist, size_t AllFiles_Pos, vector<string>& RemovedFiles, size_t& Path_Pos, string& FileName_Template, size_t& FrameCount, string& FileName_StartNumber, string& FileName_EndNumber, bitset<Action_Max> const& Actions, errors* Errors)
{
    string FN(Files[AllFiles_Pos]);
    string After;
    string Before;

    size_t After_Pos = FN.find_last_of("0123456789");
    if (After_Pos != (size_t)-1)
    {
        After = FN.substr(After_Pos + 1);
        FN.resize(After_Pos + 1);
    }

    size_t Before_Pos = FN.find_last_not_of("0123456789");
    if (Before_Pos + 9 < After_Pos)
        Before_Pos = After_Pos - 9;
    if (Before_Pos != (size_t)-1)
    {
        Before = FN.substr(0, Before_Pos + 1);
        FN.erase(0, Before_Pos + 1);
    }

    size_t AllFiles_PosToDelete = AllFiles_Pos + 1;
    bool KeepFirst = true;
    if (!FN.empty())
    {
        FileName_StartNumber = FN;
        for (;;)
        {
            RemovedFiles.push_back(Before + FN + After);
            FileName_EndNumber = FN;
            size_t i = FN.size() - 1;
            for (;;)
            {
                if (FN[i] != '9')
                {
                    FN[i]++;
                    break;
                }
                FN[i] = '0';
                if (!i)
                    break;
                i--;
            }
            string FullPath = Before + FN + After;

            if (CheckIfFilesExist)
            {
                // File list not available, checking directly if file exists
                if (access(FullPath.c_str(), 0))
                    break;
            }
            else
            {
                // Test from already created files list
                if (AllFiles_PosToDelete >= Files.size() || FullPath != Files[AllFiles_PosToDelete])
                {
                    // Remove files from the list (except the first one, used for loop increment)
                    Files.erase(Files.begin() + AllFiles_Pos + KeepFirst, Files.begin() + AllFiles_PosToDelete);
                    if (KeepFirst)
                    {
                        AllFiles_Pos++;
                        KeepFirst = false;
                    }

                    // Check if there is more from the sequence
                    while (AllFiles_Pos < Files.size() && FullPath > Files[AllFiles_Pos])
                        AllFiles_Pos++; // Skipping files not in the expected order
                    if (AllFiles_Pos >= Files.size() || FullPath != Files[AllFiles_Pos])
                        break;

                    // Keep checking files in the sequence
                    AllFiles_PosToDelete = AllFiles_Pos;
                }
                AllFiles_PosToDelete++;
            }
        }
    }

    FrameCount = RemovedFiles.size();
    if (!FrameCount)
    {
        RemovedFiles.push_back(Files[AllFiles_Pos]);
        FileName_Template = Files[AllFiles_Pos];
    }
    else
    {
        char Size = '0' + (char)FileName_StartNumber.size();
        FileName_Template = Before + "%0" + Size + "d" + After;
    }

    // Coherency test
    if (!CheckIfFilesExist && AllFiles_Pos && AllFiles_Pos < Files.size() && Actions[Action_Coherency])
    {
        auto& File1 = Files[AllFiles_Pos - 1];
        auto& File2 = Files[AllFiles_Pos];

        if (!FN.empty() && File1.size() == File2.size() && !File2.compare(0, + Before.size(), Before) && !File2.compare(Before.size() + FN.size(), After.size(), After))
        {
            auto Number1 = stoull(FN);
            size_t Number1_Pos;
            auto Number2 = stoull(File2.substr(Before.size(), FN.size()), &Number1_Pos);
            if (Number1_Pos == FN.size() && Number1 < Number2)
            {
                if (Errors)
                {
                    Errors->Error(IO_FileInput, error::Incoherent, (error::generic::code)fileinput_issue::incoherent::FileMissing, Before.substr(Path_Pos) + FN + After);
                    if (Number1 + 1 != Number2)
                    {
                        auto FN2 = to_string(Number2 - 1);
                        FN2.insert(0, FN.size() - FN2.size(), '0');
                        Errors->Error(IO_FileInput, error::Incoherent, (error::generic::code)fileinput_issue::incoherent::FileMissing, Before.substr(Path_Pos) + FN2 + After);
                    }
                }
            }
        }

    }
}

//---------------------------------------------------------------------------
void DetectSequence_FromDir(const char* Dir_Name, vector<string>& Files);
void DetectSequence_FromDir_Sub(string Dir_Name, string File_Name, bool IsHidden, vector<string>& Files)
{
    if (File_Name != "." && File_Name != "..") // Avoid . and ..
    {
        if (Dir_Name[Dir_Name.size() - 1] != '/' && Dir_Name[Dir_Name.size() - 1] != '\\')
            Dir_Name += PathSeparator;
        string File_Name_Complete = Dir_Name;
        if (Dir_Name[Dir_Name.size() - 1] != PathSeparator)
            Dir_Name += PathSeparator;
        File_Name_Complete += File_Name;
        if (IsDir(File_Name_Complete.c_str()))
            DetectSequence_FromDir(File_Name_Complete.c_str(), Files);
        else if (!IsHidden && (File_Name_Complete.size()<29 || File_Name_Complete.rfind(".rawcooked_reversibility_data")!=File_Name_Complete.size()-29))
            Files.push_back(File_Name_Complete);
    }
}

//---------------------------------------------------------------------------
void DetectSequence_FromDir(const char* Dir_Name, vector<string>& Files)
{
    string Dir_Name2 = Dir_Name;

    #if defined(_WIN32) || defined(_WINDOWS)
        if (Dir_Name2[Dir_Name2.size() - 1] != '/' && Dir_Name2[Dir_Name2.size() - 1] != '\\')
            Dir_Name2 += PathSeparator;
        WIN32_FIND_DATAA FindFileData;
        HANDLE hFind = FindFirstFileA((Dir_Name2 + '*').c_str() , &FindFileData);
        if (hFind == INVALID_HANDLE_VALUE)
            return;
        bool ReturnValue;

        do
        {
            string File_Name(FindFileData.cFileName);
            DetectSequence_FromDir_Sub(Dir_Name2, File_Name, FindFileData.dwFileAttributes&FILE_ATTRIBUTE_HIDDEN, Files);
            ReturnValue = FindNextFileA(hFind, &FindFileData);
        }
        while (ReturnValue);

        FindClose(hFind);
    #else //WINDOWS
        DIR* Dir = opendir(Dir_Name2.c_str());
        if (!Dir)
            return;
        struct dirent *DirEnt;

        while ((DirEnt = readdir(Dir)) != NULL)
        {
            //A file
            string File_Name(DirEnt->d_name);
            DetectSequence_FromDir_Sub(Dir_Name2, File_Name, DirEnt->d_name[0]=='.', Files);
        }

        closedir(Dir);
    #endif
}

//---------------------------------------------------------------------------
int input::AnalyzeInputs(global& Global)
{
    if (Global.Inputs.empty())
        return 0; // Nothing to do

    Global.HasAtLeastOneDir = false;
    Global.HasAtLeastOneFile = false;
    bool HasMoreThanOneFile = false;
    for (int i = 0; i < Global.Inputs.size(); i++)
    {
        if (IsDir(Global.Inputs[i].c_str()))
        {
            if (Global.HasAtLeastOneDir)
            {
                cerr << "Error: input contains several directories, is it intended?\nPlease contact info@mediaarea.net if you want support of such input." << endl;
                return 1;
            }
            Global.HasAtLeastOneDir = true;

            DetectSequence_FromDir(Global.Inputs[i].c_str(), Files);
        }
        else
        {
            if (Global.HasAtLeastOneFile)
                HasMoreThanOneFile = true;
            Global.HasAtLeastOneFile = true;

            Files.push_back(Global.Inputs[i]);
        }
    }
    if (Global.HasAtLeastOneDir && HasMoreThanOneFile)
    {
        cerr << "Error: input contains a mix of directories and files, is it intended?\nPlease contact info@mediaarea.net if you want support of such input." << endl;
        return 1;
    }

    if (Files.empty())
    {
        cerr << "Error: input file names structure is not recognized.\nPlease contact info@mediaarea.net if you want support of such input." << endl;
        return 1;
    }

    if (Global.Check && HasMoreThanOneFile)
    {
        cerr << "Error: \" --check\" feature is implemented only for 1 input.\nPlease contact info@mediaarea.net if you want support of such input." << endl;
        return 1;
    }

    // RAWcooked reversibility data file name
    if (Global.rawcooked_reversibility_data_FileName.empty())
    {
        if (Global.OutputFileName.empty())
            Global.rawcooked_reversibility_data_FileName = string(Global.Inputs[0]);
        else
            Global.rawcooked_reversibility_data_FileName = Global.OutputFileName;
        if (Global.rawcooked_reversibility_data_FileName[Global.rawcooked_reversibility_data_FileName.size() - 1] == '/' || Global.rawcooked_reversibility_data_FileName[Global.rawcooked_reversibility_data_FileName.size() - 1] == '\\')
            Global.rawcooked_reversibility_data_FileName.pop_back();
        Global.rawcooked_reversibility_data_FileName += ".rawcooked_reversibility_data";
    }

    // Global path position
    Global.Path_Pos_Global = (size_t)-1;
    for (int i = 0; i < Files.size(); i++)
    {
        size_t Path_Pos;
        DetectPathPos(Files[i], Path_Pos);
        if (Global.Path_Pos_Global > Path_Pos)
            Global.Path_Pos_Global = Path_Pos;
    }

    // Keeping directory name if a directory is used even if there are subdirs
    if (Global.HasAtLeastOneDir)
    {
        size_t Path_Pos = Global.Inputs[0].size();
        if (Global.Inputs[0][Global.Inputs[0].size() - 1] == '/' || Global.Inputs[0][Global.Inputs[0].size() - 1] == '\\')
            Path_Pos--;
        if (Path_Pos)
            Path_Pos = Global.Inputs[0].find_last_of("/\\", Path_Pos - 1);
        Path_Pos++;

        if (Global.Path_Pos_Global > Path_Pos)
            Global.Path_Pos_Global = Path_Pos;
    }

    return 0;
}

//---------------------------------------------------------------------------
void input::CheckDurations(vector<double> const& Durations, vector<string> const& Durations_FileName, errors* Errors)
{
    if (Durations.empty())
        return;

    auto minmax = minmax_element(Durations.begin(), Durations.end());
    if (*minmax.first + 1 < *minmax.second) // Difference of 1 second, TODO: tweakable value
    {
        if (Errors)
        {
            Errors->Error(IO_FileInput, error::Incoherent, (error::generic::code)fileinput_issue::incoherent::Duration, Durations_FileName[minmax.first - Durations.begin()] + " (" + to_string(*minmax.first) + "s)");
            Errors->Error(IO_FileInput, error::Incoherent, (error::generic::code)fileinput_issue::incoherent::Duration, Durations_FileName[minmax.second - Durations.begin()] + " (" + to_string(*minmax.second) + "s)");
        }
    }
}

//---------------------------------------------------------------------------
bool input::OpenInput(filemap& FileMap, const string& Name, errors* Errors)
{
    if (FileMap.Open_ReadMode(Name))
    {
        if (Errors)
        {
            Errors->Error(IO_FileInput, error::Undecodable, (error::generic::code)fileinput_issue::undecodable::FileCanNotBeOpen, Name);
        }
        return true;
    }

    return false;
}
