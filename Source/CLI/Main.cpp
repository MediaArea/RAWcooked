/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
*
*  Use of this source code is governed by a BSD-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#include "CLI/Help.h"
#include "Lib/Matroska/Matroska_Common.h"
#include "Lib/DPX/DPX.h"
#include "Lib/RIFF/RIFF.h"
#include "Lib/FFV1/FFV1_Frame.h"
#include "Lib/RawFrame/RawFrame.h"
#include <map>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#if defined(_WIN32) || defined(_WINDOWS)
    #include "windows.h"
    #include <io.h> // Quick and dirty for file existence
    #include <direct.h> // Quick and dirty for directory creation
    #define access _access_s
    #define mkdir _mkdir
    #define stat _stat
    #define getcwd _getcwd
    #define	S_ISDIR(m)	((m & 0170000) == 0040000)
    const char PathSeparator = '\\';
#else
    #include <dirent.h>
    #include <fcntl.h>
    #include <glob.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/mman.h>
    const char PathSeparator = '/';
#endif
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
struct ffmpeg_info_struct
{
    string FileName;
    string FileName_StartNumber;
    string FileName_Template;
    string Slices;
};
std::vector<ffmpeg_info_struct> FFmpeg_Info;
struct ffmpeg_attachment_struct
{
    string FileName_In; // Complete path
    string FileName_Out; // Relative path
};
std::vector<ffmpeg_attachment_struct> FFmpeg_Attachments;
bool IsFirstFile;
size_t Path_Pos_Global;
string rawcooked_reversibility_data_FileName;
map<string, string> Options;
size_t AttachementSize;
string OutputFileName;
string RawcookedFileName;

//---------------------------------------------------------------------------
bool IsDir(const char* Name)
{
    struct stat buffer;
    int status=stat(Name, &buffer);
    return (status==0 && S_ISDIR(buffer.st_mode));
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
        else if (!IsHidden && File_Name_Complete.rfind(".rawcooked_reversibility_data")!=File_Name_Complete.size()-29)
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
void WriteToDisk(uint64_t ID, raw_frame* RawFrame, void* Opaque)
{
    write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)Opaque;

    stringstream OutFileName;
    OutFileName << WriteToDisk_Data->FileName << ".RAWcooked" << PathSeparator << WriteToDisk_Data->FileNameDPX;
    string OutFileNameS = OutFileName.str().c_str();
    FILE* F = fopen(OutFileNameS.c_str(), (RawFrame->Buffer && !RawFrame->Pre) ? "ab" : "wb");
    if (!F)
    {
        size_t i = 0;
        for (;;)
        {
            i = OutFileNameS.find_first_of("/\\", i+1);
            if (i == (size_t)-1)
                break;
            string t = OutFileNameS.substr(0, i);
            if (access(t.c_str(), 0))
            {
                #if defined(_WIN32) || defined(_WINDOWS)
                if (mkdir(t.c_str()))
                #else
                if (mkdir(t.c_str(), 0755))
                #endif
                    exit(0);
            }
        }
        F = fopen(OutFileName.str().c_str(), (RawFrame->Buffer && !RawFrame->Pre) ?"ab":"wb");
    }
    if (RawFrame->Pre)
        fwrite(RawFrame->Pre, RawFrame->Pre_Size, 1, F);
    if (RawFrame->Buffer)
        fwrite(RawFrame->Buffer, RawFrame->Buffer_Size, 1, F);
    for (size_t p = 0; p<RawFrame->Planes.size(); p++)
        fwrite(RawFrame->Planes[p]->Buffer, RawFrame->Planes[p]->Buffer_Size, 1, F);
    if (RawFrame->Post)
        fwrite(RawFrame->Post, RawFrame->Post_Size, 1, F);
    fclose(F);
}

//---------------------------------------------------------------------------
void WriteToDisk(uint8_t* Buffer, size_t Buffer_Size, void* Opaque)
{
    write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)Opaque;

    FILE* F = fopen(rawcooked_reversibility_data_FileName.c_str(), (WriteToDisk_Data->IsFirstFile && WriteToDisk_Data->IsFirstFrame)?"wb":"ab");
    fwrite(Buffer, Buffer_Size, 1, F);
    fclose(F);
}

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

void DetectSequence(vector<string>& AllFiles, size_t AllFiles_Pos, vector<string>& Files, size_t& Path_Pos, string& FileName_Template, string& FileName_StartNumber)
{
    string FN(AllFiles[AllFiles_Pos]);
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
            Files.push_back(Path + Before + FN + After);
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
            if (AllFiles_PosToDelete + 1 < AllFiles.size())
            {
                // Test from allready created file list
                if (FullPath == AllFiles[AllFiles_PosToDelete + 1])
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
            AllFiles.erase(AllFiles.begin() + AllFiles_Pos, AllFiles.begin() + AllFiles_PosToDelete);
    }

    if (Files.empty())
    {
        Files.push_back(AllFiles[AllFiles_Pos]);
        FileName_Template = AllFiles[AllFiles_Pos];
    }
    else
    {
        char Size = '0' + (char)FN.size();
        FileName_Template = Path + Before + "%0" + Size + "d" + After;
    }

}

//---------------------------------------------------------------------------
int ParseFile(vector<string>& AllFiles, size_t AllFiles_Pos)
{
    string Name = AllFiles[AllFiles_Pos];
    
    write_to_disk_struct WriteToDisk_Data;
    WriteToDisk_Data.IsFirstFile = IsFirstFile;
    WriteToDisk_Data.FileName = Name;

    unsigned char* Buffer;
    size_t Buffer_Size;
    bool FileIsOpen;
    #if defined(_WIN32) || defined(_WINDOWS)
        HANDLE file = CreateFileA(Name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        HANDLE mapping;
        if (file != INVALID_HANDLE_VALUE)
        {
            DWORD FileSizeHigh;
            mapping = CreateFileMapping(file, 0, PAGE_READONLY, 0, 0, 0);
            Buffer = (unsigned char*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
            Buffer_Size = GetFileSize(file, &FileSizeHigh);
            if (Buffer_Size != INVALID_FILE_SIZE || GetLastError() == NO_ERROR)
                Buffer_Size |= ((size_t)FileSizeHigh) << 32;
            FileIsOpen = true;
        }
        else
            FileIsOpen = false;
    #else
        struct stat Fstat;
        stat(Name.c_str(), &Fstat);
        Buffer_Size = Fstat.st_size;
        int F = open(Name.c_str(), O_RDONLY, 0);
        if (F != -1)
        {
            Buffer = (unsigned char*)mmap(NULL, Buffer_Size, PROT_READ, MAP_FILE | MAP_PRIVATE, F, 0);
            if (Buffer != MAP_FAILED || Buffer_Size == 0)
                FileIsOpen = true;
            else
                FileIsOpen = false;
        }
        else
            FileIsOpen = false;
    #endif
    if (!FileIsOpen)
    {
        cerr << "Cannot open " << Name << endl;
        return 1;
    }

    matroska M;
    M.WriteFrameCall = &WriteToDisk;
    M.WriteFrameCall_Opaque = (void*)&WriteToDisk_Data;
    M.Buffer = Buffer;
    M.Buffer_Size = Buffer_Size;
    M.Parse();

    vector<string> Files;
    string FileName_Template;
    string FileName_StartNumber;
    string slices;

    riff RIFF;
    if (!M.IsDetected)
    {
        WriteToDisk_Data.IsFirstFrame = true;
        WriteToDisk_Data.FileNameDPX = Name.substr(Path_Pos_Global);

        RIFF.WriteFileCall = &WriteToDisk;
        RIFF.WriteFileCall_Opaque = (void*)&WriteToDisk_Data;
        RIFF.Buffer = Buffer;
        RIFF.Buffer_Size = Buffer_Size;
        RIFF.Parse();
        if (RIFF.ErrorMessage())
        {
            cerr << "Untested " << RIFF.ErrorMessage() << ", please contact info@mediaarea.net if you want support of such file\n";
            return 1;
        }

        if (RIFF.IsDetected)
        {
            IsFirstFile = false;
            Files.push_back(Name);
        }
    }

    dpx DPX;
    if (!M.IsDetected && !RIFF.IsDetected)
    {
        DetectSequence(AllFiles, AllFiles_Pos, Files, Path_Pos_Global, FileName_Template, FileName_StartNumber);

        size_t i = 0;
        WriteToDisk_Data.IsFirstFrame = true;
        for (;;)
        {
            WriteToDisk_Data.FileNameDPX = Name.substr(Path_Pos_Global);

            DPX.WriteFileCall = &WriteToDisk;
            DPX.WriteFileCall_Opaque = (void*)&WriteToDisk_Data;
            DPX.Buffer = Buffer;
            DPX.Buffer_Size = Buffer_Size;
            DPX.Parse();
            if (DPX.ErrorMessage())
            {
                cerr << "Untested " << DPX.ErrorMessage() << ", please contact info@mediaarea.net if you want support of such file\n";
                return 1;
            }
            if (!DPX.IsDetected)
                break;
            i++;
            if (WriteToDisk_Data.IsFirstFrame)
            {
                stringstream t;
                t << DPX.slice_x * DPX.slice_y;
                slices = t.str();
            }
            WriteToDisk_Data.IsFirstFrame = false;

            if (i >= Files.size())
                break;
            Name = Files[i];

            //TODO: remove duplicated code
            #if defined(_WIN32) || defined(_WINDOWS)
                UnmapViewOfFile(Buffer);
                CloseHandle(mapping);
                CloseHandle(file);
                file = CreateFileA(Name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
                Buffer_Size = GetFileSize(file, 0);
                mapping = CreateFileMapping(file, 0, PAGE_READONLY, 0, 0, 0);
                Buffer = (unsigned char*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
            #else
                munmap(Buffer, Buffer_Size);
                close(F);
                stat(Name.c_str(), &Fstat);
                size_t Buffer_Size = Fstat.st_size;
                F = open(Name.c_str(), O_RDONLY, 0);
                Buffer = (unsigned char*)mmap(NULL, Buffer_Size, PROT_READ, MAP_FILE|MAP_PRIVATE, F, 0);
            #endif
        }

        if (DPX.IsDetected)
            IsFirstFile = false;
    }

    M.Shutdown();

    #if defined(_WIN32) || defined(_WINDOWS)
        UnmapViewOfFile(Buffer);
        CloseHandle(mapping);
        CloseHandle(file);
    #else
        munmap(Buffer, Buffer_Size);
        close(F);
    #endif

    if (M.ErrorMessage())
    {
        cerr << "Untested " << M.ErrorMessage() << ", please contact info@mediaarea.net if you want support of such file\n";
        return 1;
    }

    // Processing DPX to MKV/FFV1
    if (!M.IsDetected)
    {
        if (!M.IsDetected && !RIFF.IsDetected && !DPX.IsDetected)
        {
            size_t AttachementSizeFinal = (AttachementSize != (size_t)-1) ? AttachementSize : (1024 * 1024); // Default value arbitrary choosen
            if (Buffer_Size >= AttachementSizeFinal)
            {
                cout << Name << " is not small, expected to be an attachment? Please contact info@mediaarea.net if you want support of such file.\n";
                exit(1);
            }

            if (Buffer_Size) // Ignoring file with size of 0
            {
                ffmpeg_attachment_struct Attachment;
                Attachment.FileName_In = Name;
                Attachment.FileName_Out = Name.substr(Path_Pos_Global);
                FFmpeg_Attachments.push_back(Attachment);
            }
        }
        else
        {
        ffmpeg_info_struct info;

        info.FileName = Files[0];
        if (!FileName_StartNumber.empty() && !FileName_Template.empty())
        {
            info.FileName_StartNumber = FileName_StartNumber;
            info.FileName_Template = FileName_Template;
        }

        info.Slices = slices;

        FFmpeg_Info.push_back(info);
        }

    }
    else
        cout << "Files are in " << Name << ".RAWcooked" << endl;

    return 0;
}

int FFmpeg_Command(const char* FileName)
{
    // Defaults
    if (Options.find("c:a") == Options.end())
        Options["c:a"] = "copy"; // Audio is copied
    if (Options.find("c:v") == Options.end())
        Options["c:v"] = "ffv1"; // Video format FFV1
    if (Options.find("coder") == Options.end())
        Options["coder"] = "1"; // Range Coder
    if (Options.find("context") == Options.end())
        Options["context"] = "1"; // small context
    if (Options.find("f") == Options.end())
        Options["f"] = "matroska"; // Container format Matroska
    if (Options.find("g") == Options.end())
        Options["g"] = "1"; // Intra
    if (Options.find("level") == Options.end())
        Options["level"] = "3"; // FFV1 v3
    if (Options.find("slicecrc") == Options.end())
        Options["slicecrc"] = "1"; // slice CRC on
    if (Options.find("slices") == Options.end())
    {
        // Slice count depends on frame size
        string Slices;
        for (size_t i = 0; i < FFmpeg_Info.size(); i++)
        {
            if (Slices.empty() && !FFmpeg_Info[i].Slices.empty())
                Slices = FFmpeg_Info[i].Slices;
            if (!FFmpeg_Info[i].Slices.empty() && FFmpeg_Info[i].Slices != Slices)
            {
                cerr << "Untested multiple slices counts, please contact info@mediaarea.net if you want support of such file\n";
                return 1;
            }
        }
        Options["slices"] = Slices;
    }

    string Command;
    Command += "ffmpeg";

    // Input
    for (size_t i = 0; i < FFmpeg_Info.size(); i++)
    {
        // FileName_StartNumber (if needed)
        if (!FFmpeg_Info[i].FileName_StartNumber.empty())
        {
            Command += " -start_number ";
            Command += FFmpeg_Info[i].FileName_StartNumber;
        }

        // FileName_Template (is the file name if no sequence detected)
        Command += " -i \"";
        Command += FFmpeg_Info[i].FileName_Template.empty() ? FFmpeg_Info[i].FileName : FFmpeg_Info[i].FileName_Template;
        Command += "\"";
    }

    // Map when there are several streams
    size_t MapPos = 0;
    if (FFmpeg_Info.size()>1)
    {
        for (size_t i = 0; i < FFmpeg_Info.size(); i++)
        {
            stringstream t;
            t << MapPos++;
            Command += " -map ";
            Command += t.str();
        }
    }
    else
        MapPos++;

    // Output
    for (map<string, string>::iterator Option = Options.begin(); Option != Options.end(); Option++)
        Command += " -" + Option->first + ' ' + Option->second;
    for (size_t i = 0; i < FFmpeg_Attachments.size(); i++)
    {
        stringstream t;
        t << MapPos++;
        Command += " -attach \"" + FFmpeg_Attachments[i].FileName_In + "\" -metadata:s:" + t.str() + " mimetype=application/octet-stream -metadata:s:" + t.str() + " \"filename=" + FFmpeg_Attachments[i].FileName_Out + "\"";
    }
    stringstream t;
    t << MapPos++;
    Command += " -attach \"" + rawcooked_reversibility_data_FileName + "\" -metadata:s:" + t.str() + " mimetype=application/octet-stream -metadata:s:" + t.str() + " \"filename=RAWcooked reversibility data\" \"";
    if (OutputFileName.empty())
    {
        Command += FileName;
        if (Command[Command.size() - 1] == '/' || Command[Command.size() - 1] == '\\')
            Command.pop_back();
        Command += ".mkv";
    }
    else
    {
        Command += OutputFileName;
    }
    Command += '\"';
    cout << Command << endl;

    return 0;
}

//---------------------------------------------------------------------------
int Error_NotTested(const char* Option1, const char* Option2 = NULL)
{
    cerr << "Option " << Option1;
    if (Option2)
        cerr << ' ' << Option2;
    cerr << " not yet tested, please contact info@mediaarea.net if you want support of such option\n";
    return 1;
}

int SetOption(const char* argv[], int& i, int argc)
{
    if (strcmp(argv[i], "-c:a") == 0)
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (strcmp(argv[i], "copy") == 0)
        {
            Options["c:a"] = argv[i];
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
            Options["c:v"] = argv[i];
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
            Options["coder"] = argv[i];
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
            Options["context"] = argv[i];
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
            Options["f"] = argv[i];
            return 0;
        }
        return 0;
    }
    if (!strcmp(argv[i], "-g"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "0")
         || !strcmp(argv[i], "1"))
        {
            Options["g"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-level"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "0")
            || !strcmp(argv[i], "1")
            || !strcmp(argv[i], "3"))
        {
            Options["level"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-slicecrc"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        int SliceCount = atoi(argv[i]);
        if (!strcmp(argv[i], "0")
         || !strcmp(argv[i], "1"))
        {
            Options["slicecrc"] = argv[i];
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
            Options["slices"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-threads"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        Options["threads"] = argv[i];
        return 0;
    }

    return Error_NotTested(argv[i]);
}

//---------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
    if (argc < 2)
        return Usage(argv[0]);
    
    AttachementSize = (size_t)-1;

    vector<string> Args;
    for (int i = 0; i < argc; i++)
    {
        if ((argv[i][0] == '.' && argv[i][1] == '\0')
            || (argv[i][0] == '.' && (argv[i][1] == '/' || argv[i][1] == '\\') && argv[i][2] == '\0'))
        {
            //translate to "../xxx" in order to get the top level directory name
            char buff[FILENAME_MAX];
            getcwd(buff, FILENAME_MAX);
            string Arg = buff;
            size_t Path_Pos = Arg.find_last_of("/\\");
            Arg = ".." + Arg.substr(Path_Pos);
            Args.push_back(Arg);
        }
        else if ((strcmp(argv[i], "--attachment-size") == 0 || strcmp(argv[i], "-s") == 0) && i + 1 < argc)
            AttachementSize = atoi(argv[++i]);
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
            return Help(argv[0]);
        else if (strcmp(argv[i], "--version") == 0)
            return Version();
        else if ((strcmp(argv[i], "--output-file-name") == 0 || strcmp(argv[i], "-o") == 0) && i + 1 < argc)
            OutputFileName = argv[++i];
        else if ((strcmp(argv[i], "--rawcooked-file-name") == 0 || strcmp(argv[i], "-r") == 0) && i + 1 < argc)
            RawcookedFileName = argv[++i];
        else if (argv[i][0] == '-' && argv[i][1] != '\0')
        {
            int Value = SetOption(argv, i, argc);
            if (Value)
                return Value;
        }
        else
            Args.push_back(argv[i]);
    }

    vector<string> Files;
    bool HasAtLeastOneDir = false;
    bool HasAtLeastOneFile = false;
    for (int i = 1; i < Args.size(); i++)
    {
        if (IsDir(Args[i].c_str()))
        {
            if (HasAtLeastOneDir)
            {
                cout << "Input contains several directories, is it intended? Please contact info@mediaarea.net if you want support of such input.\n";
                return 1;
            }
            HasAtLeastOneDir = true;

            DetectSequence_FromDir(Args[i].c_str(), Files);
        }
        else
        {
            HasAtLeastOneFile = true;

            Files.push_back(Args[i]);
        }
    }
    if (HasAtLeastOneDir && HasAtLeastOneFile)
    {
        cout << "Input contains a mix of directories and files, is it intended? Please contact info@mediaarea.net if you want support of such input.\n";
        return 1;
    }

    // RAWcooked file name
    if (RawcookedFileName.empty())
    {
        rawcooked_reversibility_data_FileName = string(Args[1]);
        if (rawcooked_reversibility_data_FileName[rawcooked_reversibility_data_FileName.size() - 1] == '/' || rawcooked_reversibility_data_FileName[rawcooked_reversibility_data_FileName.size() - 1] == '\\')
            rawcooked_reversibility_data_FileName.pop_back();
        rawcooked_reversibility_data_FileName += ".rawcooked_reversibility_data";
    }
    else
        rawcooked_reversibility_data_FileName = RawcookedFileName;

    // Global path position
    Path_Pos_Global = (size_t)-1;
    for (int i = 0; i < Files.size(); i++)
    {
        size_t Path_Pos;
        DetectPathPos(Files[i], Path_Pos);
        if (Path_Pos_Global > Path_Pos)
            Path_Pos_Global = Path_Pos;
    }

    // Keeping directory name if a directory is used even if there are subdirs
    if (HasAtLeastOneDir)
    {
        size_t Path_Pos = Args[1].size();
        if (Args[1][Args[1].size() - 1] == '/' || Args[1][Args[1].size() - 1] == '\\')
            Path_Pos--;
        if (Path_Pos)
            Path_Pos = Args[1].find_last_of("/\\", Path_Pos - 1);
        if (Path_Pos == string::npos)
            Path_Pos = 0;

        if (Path_Pos_Global > Path_Pos)
            Path_Pos_Global = Path_Pos;
    }

    // Parsing files
    IsFirstFile = true;
    for (int i = 0; i < Files.size(); i++)
        if (ParseFile(Files, i))
            return 1;

    // FFmpeg
    if (!FFmpeg_Info.empty())
        return FFmpeg_Command(Args[1].c_str());
    else if (!FFmpeg_Attachments.empty())
    {
        cerr << "No A/V content detected, please contact info@mediaarea.net if you want support of such content\n";
        return 1;
    }

    return 0;
}
