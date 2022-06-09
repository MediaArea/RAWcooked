/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/Input.h"
#include "Lib/Common/Common.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include "Lib/ThirdParty/alphanum/alphanum.hpp"
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
    "file names (sequence names e.g. 09 and 9)",
};

enum code : uint8_t
{
    FileCanNotBeOpen,
    FileNameSequence,
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

static_assert(error::type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

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
void input::DetectSequence(bool CheckIfFilesExist, size_t AllFiles_Pos, vector<string>& RemovedFiles, size_t& Path_Pos, string& FileName_Template, string& FileName_StartNumber, string& FileName_EndNumber, string& FileList, bitset<Action_Max> const& Actions, errors* Errors)
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
    bool MustCreateFileList = false;
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
                {
                    FN.insert(0, 1, '1');
                    break;
                }
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
                    while (AllFiles_Pos < Files.size() && doj::alphanum_comp(FullPath, Files[AllFiles_Pos]) > 0)
                    {
                        // Test of unsupported file names e.g. 09 and 9 in the list
                        size_t Pos1 = FN.size();
                        size_t j = Pos1 - 1;
                        for (;;)
                        {
                            switch (FN[j])
                            {
                                case '0':
                                    break;
                                case '1':
                                    Pos1 = j;
                                    break;
                                default:
                                    Pos1 = FN.size();
                                    j = 0;
                            }
                            if (!j)
                                break;
                            j--;
                        }
                        if (Pos1 < FN.size())
                        {
                            string FN2;
                            Pos1++;
                            FN2.resize(Pos1, '0');
                            FN2.resize(FN.size(), '9');
                            auto FN3 = FN2;
                            while (!FN3.empty() && FN3[0] == '0')
                            {
                                FN3.erase(0, 1);
                                if (Before + FN3 + After == Files[AllFiles_Pos])
                                {
                                    Errors->Error(IO_FileInput, error::type::Incoherent, (error::generic::code)fileinput_issue::undecodable::FileNameSequence, Before.substr(Path_Pos) + FN2 + After);
                                    Errors->Error(IO_FileInput, error::type::Incoherent, (error::generic::code)fileinput_issue::undecodable::FileNameSequence, Before.substr(Path_Pos) + FN3 + After);
                                }
                            }
                        }

                        AllFiles_Pos++; // Skipping files not in the expected order
                    }
                    if (AllFiles_Pos >= Files.size())
                        break;
                    if (FullPath != Files[AllFiles_Pos])
                    {
                        // Coherency test
                        auto NextFileFound = false;
                        for (auto AllFiles_Pos_Next = AllFiles_Pos; AllFiles_Pos_Next < Files.size(); AllFiles_Pos_Next++)
                        {
                            auto& File2 = Files[AllFiles_Pos_Next];
                            if (!File2.compare(0, Before.size(), Before)) // Same start
                            {
                                if (File2.size() < Before.size() + FN.size() + After.size()) // Not enough characters for storing the expected string
                                    continue;
                                auto End = File2.size() - After.size();
                                if (File2.compare(End, After.size(), After)) // Not same end
                                    continue;
                                auto TestDigit = Before.size();
                                do
                                {
                                    const auto& Char = File2[TestDigit];
                                    if (Char < '0' || Char > '9')
                                        break;
                                    TestDigit++;
                                } while (TestDigit < End);
                                if (TestDigit == End)
                                {
                                    auto Number1 = stoull(FN);
                                    auto Number2 = stoull(File2.substr(Before.size(), TestDigit - Before.size()));
                                    if (Number1 < Number2)
                                    {
                                        if (Actions[Action_Coherency] && !Actions[Action_AcceptGaps] && Errors)
                                        {
                                            Errors->Error(IO_FileInput, error::type::Incoherent, (error::generic::code)fileinput_issue::incoherent::FileMissing, Before.substr(Path_Pos) + FN + After);
                                            if (Number1 + 1 != Number2)
                                            {
                                                auto FN2 = to_string(Number2 - 1);
                                                if (FN.size() > FN2.size())
                                                    FN2.insert(0, FN.size() - FN2.size(), '0');
                                                if (Number1 + 2 != Number2)
                                                    Errors->Error(IO_FileInput, error::type::Incoherent, (error::generic::code)fileinput_issue::incoherent::FileMissing, "...");
                                                Errors->Error(IO_FileInput, error::type::Incoherent, (error::generic::code)fileinput_issue::incoherent::FileMissing, Before.substr(Path_Pos) + FN2 + After);
                                            }
                                        }
                                        FN = File2.substr(Before.size(), TestDigit - Before.size());
                                        NextFileFound = true;
                                        MustCreateFileList = true;
                                    }
                                    break;
                                }
                            }
                        }
                        if (!NextFileFound)
                            break;
                    }

                    // Keep checking files in the sequence
                    AllFiles_PosToDelete = AllFiles_Pos;
                }
                AllFiles_PosToDelete++;
            }
        }
    }

    if (RemovedFiles.empty())
    {
        RemovedFiles.push_back(Files[AllFiles_Pos]);
        FileName_Template = Files[AllFiles_Pos];
    }
    else
    {
        char Size = '0' + (char)FileName_StartNumber.size();
        FileName_Template = Before + "%0" + Size + "d" + After;
        if (MustCreateFileList && !RemovedFiles.empty())
        {
            FileList.reserve(RemovedFiles.begin()->size() * RemovedFiles.size());
            for (auto File : RemovedFiles)
            {
                FileList += File;
                FileList += '\n';
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
    for (size_t i = 0; i < Global.Inputs.size(); i++)
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

    if (Global.Actions[Action_Check] && HasMoreThanOneFile)
    {
        cerr << "Error: \" --check\" feature is implemented only for 1 input.\nPlease contact info@mediaarea.net if you want support of such input." << endl;
        return 1;
    }

    // RAWcooked reversibility data file name
    if (Global.rawcooked_reversibility_FileName.empty())
    {
        if (Global.OutputFileName.empty())
            Global.rawcooked_reversibility_FileName = string(Global.Inputs[0]);
        else
            Global.rawcooked_reversibility_FileName = Global.OutputFileName;
        if (Global.rawcooked_reversibility_FileName[Global.rawcooked_reversibility_FileName.size() - 1] == '/' || Global.rawcooked_reversibility_FileName[Global.rawcooked_reversibility_FileName.size() - 1] == '\\')
            Global.rawcooked_reversibility_FileName.pop_back();
        Global.rawcooked_reversibility_FileName += ".rawcooked_reversibility_data";
    }

    // Global path position
    Global.Path_Pos_Global = (size_t)-1;
    for (size_t i = 0; i < Files.size(); i++)
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
            Errors->Error(IO_FileInput, error::type::Incoherent, (error::generic::code)fileinput_issue::incoherent::Duration, Durations_FileName[minmax.first - Durations.begin()] + " (" + to_string(*minmax.first) + "s)");
            Errors->Error(IO_FileInput, error::type::Incoherent, (error::generic::code)fileinput_issue::incoherent::Duration, Durations_FileName[minmax.second - Durations.begin()] + " (" + to_string(*minmax.second) + "s)");
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
            Errors->Error(IO_FileInput, error::type::Undecodable, (error::generic::code)fileinput_issue::undecodable::FileCanNotBeOpen, Name);
        }
        return true;
    }

    return false;
}
