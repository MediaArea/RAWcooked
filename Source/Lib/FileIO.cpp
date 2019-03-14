/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/FileIO.h"
#include <iostream>
#include <sstream>
#if defined(_WIN32) || defined(_WINDOWS)
    #include "windows.h"
    #include <io.h> // File existence
    #include <direct.h> // Directory creation
    #define access _access_s
    #define mkdir _mkdir
    #define stat _stat
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
// Platform specific data
#if defined(_WIN32) || defined(_WINDOWS)
    struct private_data
    {
        HANDLE File;
        HANDLE Mapping;
    };
#else
    struct private_data
    {
        int File;
    };
#endif

//---------------------------------------------------------------------------
filemap::filemap() :
    Buffer(NULL),
    Buffer_Size(0)
{
    Private = new private_data;
}

//---------------------------------------------------------------------------
filemap::~filemap()
{
    Close();
    delete (private_data*)Private;
}

//---------------------------------------------------------------------------
int filemap::Open_ReadMode(const char* FileName)
{
    Close();

    private_data& P=*((private_data*)Private);
    bool FileIsOpen;

    #if defined(_WIN32) || defined(_WINDOWS)
        P.File = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (P.File != INVALID_HANDLE_VALUE)
        {
            DWORD FileSizeHigh;
            Buffer_Size = GetFileSize(P.File, &FileSizeHigh);
            if (Buffer_Size != INVALID_FILE_SIZE || GetLastError() == NO_ERROR)
            {
                Buffer_Size |= ((size_t)FileSizeHigh) << 32;
                if (Buffer_Size)
                {
                    P.Mapping = CreateFileMapping(P.File, 0, PAGE_READONLY, 0, 0, 0);
                    if (P.Mapping)
                        FileIsOpen = true;
                    else
                    {
                        CloseHandle(P.File);
                        P.File = INVALID_HANDLE_VALUE;
                        Buffer_Size = 0;
                        FileIsOpen = false;
                    }
                }
                else
                    FileIsOpen = true; // MapViewOfFile does not support 0-byte files, so we map manually to NULL
            }
            else
            {
                CloseHandle(P.File);
                P.File = INVALID_HANDLE_VALUE;
                FileIsOpen = false;
            }
        }
        else
            FileIsOpen = false;
    #else
        struct stat Fstat;
        if (!stat(FileName, &Fstat))
        {
            Buffer_Size = Fstat.st_size;
            if (Buffer_Size)
            {
                P.File = open(FileName, O_RDONLY, 0);
                if (P.File != -1)
                    FileIsOpen = true;
                else
                {
                    Buffer_Size = 0;
                    FileIsOpen = false;
                }
            }
            else
                FileIsOpen = true; // mmap does not support 0-byte files, so we map manually to NULL
        }
        else
            FileIsOpen = false;

    #endif

    if (FileIsOpen)
        FileIsOpen = Remap() ? false : true;

    if (!FileIsOpen)
    {
        cerr << "Cannot open " << FileName << endl;
        return 1;
    }

    return 0;
}

//---------------------------------------------------------------------------
int filemap::Remap()
{
    if (!Buffer_Size)
        return 0;

    private_data& P = *((private_data*)Private);

    #if defined(_WIN32) || defined(_WINDOWS)
        UnmapViewOfFile(Buffer);
        Buffer = (unsigned char*)MapViewOfFile(P.Mapping, FILE_MAP_READ, 0, 0, 0);
        if (!Buffer)
        {
            CloseHandle(P.Mapping);
            CloseHandle(P.File);
            P.Mapping = NULL;
            P.File = INVALID_HANDLE_VALUE;
            Buffer_Size = 0;
            return 1;
        }
    #else
        if (Buffer)
            munmap(Buffer, Buffer_Size);
        Buffer = (unsigned char*)mmap(NULL, Buffer_Size, PROT_READ, MAP_FILE | MAP_PRIVATE, P.File, 0);
        if (Buffer == MAP_FAILED)
        {
            close(P.File);
            Buffer = NULL;
            Buffer_Size = 0;
            return 1;
        }
    #endif

    return 0;
}

//---------------------------------------------------------------------------
int filemap::Close()
{
    if (!Buffer)
        return 0;

    private_data& P = *((private_data*)Private);

    #if defined(_WIN32) || defined(_WINDOWS)
        UnmapViewOfFile(Buffer);
        CloseHandle(P.Mapping);
        CloseHandle(P.File);
        P.Mapping = NULL;
        P.File = INVALID_HANDLE_VALUE;
    #else
        munmap(Buffer, Buffer_Size);
        close(P.File);
        P.File = -1;
    #endif

    Buffer = NULL;
    Buffer_Size = 0;

    return 0;
}

//---------------------------------------------------------------------------
// Errors

namespace file_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "can not create directory",
    "can not open file for writing",
    "can not write in the file",
};

enum code : uint8_t
{
    CreateDirectory,
    FileOpenWriting,
    FileWrite,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unparsable

const char** ErrorTexts[] =
{
    undecodable::MessageText,
    nullptr,
};

static_assert(error::Type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // file_issue

//---------------------------------------------------------------------------
// file

bool file::Open(const string& BaseDirectory, const string& OutputFileName_Source, const char* Mode)
{
    OutputFileName = OutputFileName_Source;
    string FullName = BaseDirectory + OutputFileName;

    #ifdef _MSC_VER
        #pragma warning(disable:4996)// _CRT_SECURE_NO_WARNINGS
    #endif
    FILE* F = fopen(FullName.c_str(), Mode);
    #ifdef _MSC_VER
        #pragma warning(default:4996)// _CRT_SECURE_NO_WARNINGS
    #endif
    if (!F)
    {
        size_t i = 0;
        for (;;)
        {
            i = FullName.find_first_of("/\\", i + 1);
            if (i == (size_t)-1)
                break;
            string t = FullName.substr(0, i);
            if (access(t.c_str(), 0))
            {
                #if defined(_WIN32) || defined(_WINDOWS)
                if (mkdir(t.c_str()))
                #else
                if (mkdir(t.c_str(), 0755))
                #endif
                {
                    if (Errors)
                        Errors->Error(Parser_FileWriter, error::Undecodable, (error::generic::code)file_issue::undecodable::CreateDirectory, OutputFileName.substr(0, i));
                    return true;
                }
            }
        }
        #ifdef _MSC_VER
            #pragma warning(disable:4996) // _CRT_SECURE_NO_WARNINGS
        #endif
        F = fopen(FullName.c_str(), Mode);
        #ifdef _MSC_VER
            #pragma warning(default:4996) // _CRT_SECURE_NO_WARNINGS
        #endif
        if (!F)
        {
            if (Errors)
                Errors->Error(Parser_FileWriter, error::Undecodable, (error::generic::code)file_issue::undecodable::FileOpenWriting, OutputFileName);
            return true;
        }
    }

    Private = F;
    return false;
}

//---------------------------------------------------------------------------
// file

bool file::Write(const void* Buffer, size_t Size)
{
    if (!Size)
        return false;

    if (fwrite(Buffer, Size, 1, (FILE*)Private) != 1)
    {
        if (Errors)
            Errors->Error(Parser_FileWriter, error::Undecodable, (error::generic::code)file_issue::undecodable::FileWrite, OutputFileName);
        fclose((FILE*)Private);
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------
// file

bool file::Close()
{
    if (!Private)
        return false;
        
    if (fclose((FILE*)Private))
    {
        if (Errors)
            Errors->Error(Parser_FileWriter, error::Undecodable, (error::generic::code)file_issue::undecodable::FileWrite, OutputFileName);
        return true;
    }

    Private = nullptr;
    return false;
}
