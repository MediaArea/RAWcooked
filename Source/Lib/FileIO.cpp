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