/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/Input.h"
#include <iostream>
#if defined(_WIN32) || defined(_WINDOWS)
    #include "windows.h"
    #include <io.h> // File existence
    #define access _access_s
    #define stat _stat
    #define	S_ISDIR(m)	((m & 0170000) == 0040000)
    static const char PathSeparator = '\\';
#else
    #include <dirent.h>
    #include <fcntl.h>
    #include <glob.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/mman.h>
    static const char PathSeparator = '/';
#endif
//---------------------------------------------------------------------------

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
void input::DetectSequence(size_t AllFiles_Pos, vector<string>& RemovedFiles, size_t& Path_Pos, string& FileName_Template, string& FileName_StartNumber)
{
    string FN(Files[AllFiles_Pos]);
    string Path;
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

    size_t AllFiles_PosToDelete = AllFiles_Pos;
    if (!FN.empty())
    {
        FileName_StartNumber = FN;
        for (;;)
        {
            RemovedFiles.push_back(Path + Before + FN + After);
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
            string FullPath = Path + Before + FN + After;
            if (AllFiles_PosToDelete + 1 < Files.size())
            {
                // Test from allready created file list
                if (FullPath == Files[AllFiles_PosToDelete + 1])
                    AllFiles_PosToDelete++;
                //Checking directly if file exists
                else if (access(FullPath.c_str(), 0))
                    break;
            }
            else
            {
                // File list not avalable, checking directly if file exists
                if (access(FullPath.c_str(), 0))
                    break;
            }
        }

        if (AllFiles_PosToDelete != AllFiles_Pos)
            Files.erase(Files.begin() + AllFiles_Pos, Files.begin() + AllFiles_PosToDelete);
    }

    if (RemovedFiles.empty())
    {
        RemovedFiles.push_back(Files[AllFiles_Pos]);
        FileName_Template = Files[AllFiles_Pos];
    }
    else
    {
        char Size = '0' + (char)FN.size();
        FileName_Template = Path + Before + "%0" + Size + "d" + After;
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

    bool HasAtLeastOneDir = false;
    bool HasAtLeastOneFile = false;
    for (int i = 0; i < Global.Inputs.size(); i++)
    {
        if (IsDir(Global.Inputs[i].c_str()))
        {
            if (HasAtLeastOneDir)
            {
                cerr << "Input contains several directories, is it intended? Please contact info@mediaarea.net if you want support of such input.\n";
                return 1;
            }
            HasAtLeastOneDir = true;

            DetectSequence_FromDir(Global.Inputs[i].c_str(), Files);
        }
        else
        {
            HasAtLeastOneFile = true;

            Files.push_back(Global.Inputs[i]);
        }
    }
    if (HasAtLeastOneDir && HasAtLeastOneFile)
    {
        cerr << "Input contains a mix of directories and files, is it intended? Please contact info@mediaarea.net if you want support of such input.\n";
        return 1;
    }

    if (Files.empty())
    {
        cerr << "Input file names structure is not recognized. Please contact info@mediaarea.net if you want support of such input.\n";
        return 1;
    }

    // RAWcooked reversibility data file name
    if (Global.rawcooked_reversibility_data_FileName.empty())
    {
        Global.rawcooked_reversibility_data_FileName = string(Global.Inputs[0]);
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
    if (HasAtLeastOneDir)
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