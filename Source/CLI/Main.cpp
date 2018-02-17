/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
*
*  Use of this source code is governed by a MIT-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#include "CLI/Help.h"
#include "Lib/Matroska/Matroska_Common.h"
#include "Lib/DPX/DPX.h"
#include "Lib/FFV1/FFV1_Frame.h"
#include "Lib/RawFrame/RawFrame.h"
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#if defined(_WIN32) || defined(_WINDOWS)
   #include "windows.h"
   #include <io.h>  // Quick and dirty for file existence
    #include <direct.h> // Quick and dirty for directory creation
    #define access    _access_s
    #define mkdir    _mkdir
    const char PathSeparator = '\\';
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/mman.h>
    const char PathSeparator = '/';
#endif
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void WriteToDisk(uint64_t ID, raw_frame* RawFrame, void* Opaque)
{
    write_to_disk_struct* WriteToDisk_Data = (write_to_disk_struct*)Opaque;

    stringstream OutFileName;
    OutFileName << WriteToDisk_Data->FileName << ".RAWcooked" << PathSeparator << WriteToDisk_Data->FileNameDPX;
    string OutFileNameS = OutFileName.str().c_str();
    FILE* F = fopen(OutFileNameS.c_str(), "wb");
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
        F = fopen(OutFileName.str().c_str(), "wb");
    }
    if (RawFrame->Pre)
        fwrite(RawFrame->Pre, RawFrame->Pre_Size, 1, F);
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

    string OutFileName(WriteToDisk_Data->FileName);
    size_t Path_Pos = OutFileName.rfind(PathSeparator);
    if (Path_Pos != ((size_t)-1))
        OutFileName.resize(Path_Pos + 1);
    else
        OutFileName.clear();
    OutFileName += "rawcooked.id=1"; //TODO: ID of the track
    FILE* F = fopen(OutFileName.c_str(), WriteToDisk_Data->IsFirstFrame?"wb":"ab");
    fwrite(Buffer, Buffer_Size, 1, F);
    fclose(F);
}

void DetectSequence(const char* Name, vector<string>& Files, size_t& Path_Pos, string& FileName_Template, string& FileName_StartNumber)
{
    string FN(Name);
    string Path;
    string After;
    string Before;
    Path_Pos = FN.rfind(PathSeparator);
    if (Path_Pos != (size_t)-1)
    {
        Path_Pos++;
        Path = FN.substr(0, Path_Pos);
        if (Path_Pos > 2)
        {
            size_t Path_Pos2 = FN.rfind(PathSeparator, Path_Pos - 2);
            FN.erase(0, Path_Pos);

            if (Path_Pos2 != (size_t)-1)
            {
                Path_Pos = Path_Pos2 + 1;
            }
        }
    }
    else
        Path_Pos = 0;

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
            if (access((Path + Before + FN + After).c_str(), 0)) // Dirty way to test file exitence. TODO: better way
                break;
        }
    }

    if (Files.empty())
    {
        Files.push_back(Name);
        FileName_Template = Name;
    }
    else
    {
        char Size = '0' + (char)FN.size();
        FileName_Template = Path + Before + "%0" + Size + "d" + After;
    }
    
}

//---------------------------------------------------------------------------
int ParseFile(const char* Name)
{
    write_to_disk_struct WriteToDisk_Data;
    WriteToDisk_Data.FileName = Name;
    
    #if defined(_WIN32) || defined(_WINDOWS)
        HANDLE file = CreateFileA(Name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        size_t Buffer_Size = GetFileSize(file, 0);
        HANDLE mapping = CreateFileMapping(file, 0, PAGE_READONLY, 0, 0, 0);
        unsigned char* Buffer = (unsigned char*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    #else
        struct stat Fstat;
        stat(Name, &Fstat);
        size_t Buffer_Size = Fstat.st_size;
        int F = open(Name, O_RDONLY, 0);
        unsigned char* Buffer = (unsigned char*)mmap(NULL, Buffer_Size, PROT_READ, MAP_FILE|MAP_PRIVATE, F, 0);
    #endif

    matroska M;
    M.WriteFrameCall = &WriteToDisk;
    M.WriteFrameCall_Opaque = (void*)&WriteToDisk_Data;
    M.Buffer = Buffer;
    M.Buffer_Size = Buffer_Size;
    M.Parse();

    vector<string> Files;
    string FileName_Template;
    string FileName_StartNumber;
    size_t Path_Pos=0;
    string slices;
    if (!M.IsDetected)
    {
        DetectSequence(Name, Files, Path_Pos, FileName_Template, FileName_StartNumber);
        
        size_t i = 0;
        WriteToDisk_Data.IsFirstFrame = true;
        for (;;)
        {
            WriteToDisk_Data.FileNameDPX=Name+Path_Pos;
            
            dpx DPX;
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
            i++;
            if (WriteToDisk_Data.IsFirstFrame)
            {
                stringstream t;
                t << DPX.slice_x * DPX.slice_x;
                slices = t.str();
            }
            WriteToDisk_Data.IsFirstFrame = false;

            if (i >= Files.size())
                break;
            Name = Files[i].c_str();

            //TODO: remove duplicated code
            #if defined(_WIN32) || defined(_WINDOWS)
                UnmapViewOfFile(Buffer);
                CloseHandle(mapping);
                CloseHandle(file);
                file = CreateFileA(Name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
                Buffer_Size = GetFileSize(file, 0);
                mapping = CreateFileMapping(file, 0, PAGE_READONLY, 0, 0, 0);
                Buffer = (unsigned char*)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
            #else
                munmap(Buffer, Buffer_Size);
                close(F);
                stat(Name, &Fstat);
                size_t Buffer_Size = Fstat.st_size;
                F = open(Name, O_RDONLY, 0);
                Buffer = (unsigned char*)mmap(NULL, Buffer_Size, PROT_READ, MAP_FILE|MAP_PRIVATE, F, 0);
            #endif
        }
    }

    #if defined(_WIN32) || defined(_WINDOWS)
        UnmapViewOfFile(Buffer);
        CloseHandle(mapping);
        CloseHandle(file);
    #else
        munmap(Buffer, Buffer_Size);
        close(F);
    #endif

    if (M.Frame.ErrorMessage())
    {
        cerr << "Untested " << M.Frame.ErrorMessage() << ", please contact info@mediaarea.net if you want support of such file\n";
        return 1;
    }

    // Processing DPX to MKV/FFV1
    if (!M.IsDetected)
    {
        string OutFileName(Files[0]); //TODO: remove duplicated code
        size_t Path_Pos = OutFileName.rfind(PathSeparator);
        if (Path_Pos != ((size_t)-1))
            OutFileName.resize(Path_Pos + 1);
        else
            OutFileName.clear();
        OutFileName += "rawcooked.id=1"; //TODO: ID of the track

        string Command;
        Command += "ffmpeg";
        if (!FileName_StartNumber.empty() && !FileName_Template.empty())
        {
            Command += " -start_number ";
            Command += FileName_StartNumber;
            Command += " -i \"";
            Command += FileName_Template;
            Command += "\"";
        }
        else
        {
            Command += " -i \"";
            Command += Files[0];
            Command += "\"";
        }
        Command += " -c:v ffv1 -level 3 -coder 1 -context 0 -g 1 -slices " + slices + " -strict -2 -attach \"" + OutFileName + "\" -metadata:s:t mimetype=application/octet-stream -metadata:s:t filename=rawcooked.id=1 \"";
        Command += Files[0];
        Command += ".mkv\"";

        cout << Command << endl;
    }
    else if (!M.Frame.RawFrame || M.Frame.RawFrame->Planes.empty())
        cerr << "Problem while parsing the MKV file" << endl;
    else
        cout << "Files are in " << Name << ".RAWcooked" << endl;

    return 0;
}

//---------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    if (argc < 2)
        return Usage(argv[0]);
    if ((strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "-h") == 0))
        return Help(argv[0]);

    for (int i = 1; i < argc; i++)
        if (ParseFile(argv[i]))
            return 1;

    return 0;
}

