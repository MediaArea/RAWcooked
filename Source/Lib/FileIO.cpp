/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#define _GNU_SOURCE // Needed for ftruncate on GNU compiler
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
int filemap::Open_ReadMode(const char* FileName)
{
    Close();

    bool FileIsOpen;

    #if defined(_WIN32) || defined(_WINDOWS)
        HANDLE& File = (HANDLE&)Private;
        File = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
        if (File != INVALID_HANDLE_VALUE)
        {
            DWORD FileSizeHigh;
            Buffer_Size = GetFileSize(File, &FileSizeHigh);
            if (Buffer_Size != INVALID_FILE_SIZE || GetLastError() == NO_ERROR)
            {
                Buffer_Size |= ((size_t)FileSizeHigh) << 32;
                if (Buffer_Size)
                {
                    HANDLE& Mapping = (HANDLE&)Private2;
                    Mapping = CreateFileMapping(File, 0, PAGE_READONLY, 0, 0, 0);
                    if (Mapping)
                        FileIsOpen = true;
                    else
                    {
                        Close();
                        FileIsOpen = false;
                    }
                }
                else
                    FileIsOpen = true; // CreateFileMapping does not support 0-byte files, so we map manually to NULL
            }
            else
            {
                Close();
                FileIsOpen = false;
            }
        }
        else
            FileIsOpen = false;
    #else
        int& P = (int&)Private;
        P = open(FileName, O_RDONLY, 0);
        if (P != -1)
        {
            struct stat Fstat;
            if (!stat(FileName, &Fstat))
            {
                Buffer_Size = Fstat.st_size;
                FileIsOpen = true;
            }
            else
            {
                Close();
                FileIsOpen = false;
            }
        }
        else
            FileIsOpen = false;

    #endif

    if (FileIsOpen)
        FileIsOpen = Remap() ? false : true;

    if (!FileIsOpen)
        return 1;

    return 0;
}

//---------------------------------------------------------------------------
int filemap::Remap()
{
    if (Buffer)
    {
        #if defined(_WIN32) || defined(_WINDOWS)
            UnmapViewOfFile(Buffer);
        #else
            munmap(Buffer, Buffer_Size);
        #endif
        Buffer = NULL;
    }

    if (!Buffer_Size)
      return 0;

    #if defined(_WIN32) || defined(_WINDOWS)
        HANDLE& Mapping = (HANDLE&)Private2;
        Buffer = (unsigned char*)MapViewOfFile(Mapping, FILE_MAP_READ, 0, 0, 0);
        if (!Buffer)
        {
            CloseHandle(Mapping);
            HANDLE& File = (HANDLE&)Private;
            CloseHandle(File);
            Mapping = NULL;
            File = INVALID_HANDLE_VALUE;
            Buffer_Size = 0;
            return 1;
        }
    #else
        int& P = (int&)Private;
        Buffer = (unsigned char*)mmap(NULL, Buffer_Size, PROT_READ, MAP_FILE | MAP_PRIVATE, P, 0);
        if (Buffer == MAP_FAILED)
        {
            Buffer = NULL;
            Close();
            return 1;
        }
    #endif

    return 0;
}

//---------------------------------------------------------------------------
int filemap::Close()
{
    #if defined(_WIN32) || defined(_WINDOWS)
        if (Buffer)
        {
            UnmapViewOfFile(Buffer);
            HANDLE& Mapping = (HANDLE&)Private2;
            CloseHandle(Mapping);
            Private2 = (void*)-1;
            Buffer = NULL;
            Buffer_Size = 0;
        }
        HANDLE& File = (HANDLE&)Private;
        if (File != INVALID_HANDLE_VALUE)
        {
            if (CloseHandle(File) == 0)
            {
                Private = (void*)-1;
                return 1;
            }
        }
    #else
        if (Buffer)
        {
            munmap(Buffer, Buffer_Size);
            Buffer = NULL;
            Buffer_Size = 0;
        }
        int& P = (int&)Private;
        if (P != -1)
        {
            if (close(P))
            {
                Private = (void*)-1;
                return 1;
            }
        }
    #endif

    Private = (void*)-1;
    return 0;
}

//---------------------------------------------------------------------------
// file

file::return_value file::Open_WriteMode(const string& BaseDirectory, const string& OutputFileName_Source, bool RejectIfExists, bool Truncate)
{
    Close();

    string FullName = BaseDirectory + OutputFileName_Source;

    #if defined(_WIN32) || defined(_WINDOWS)
    HANDLE& P = (HANDLE&)Private;
    DWORD CreationDisposition = Truncate ? (RejectIfExists ? TRUNCATE_EXISTING : CREATE_ALWAYS) : (RejectIfExists ? CREATE_NEW : OPEN_ALWAYS);
    P = CreateFileA(FullName.c_str(), GENERIC_WRITE, 0, 0, CreationDisposition, FILE_ATTRIBUTE_NORMAL, 0);
    if (P == INVALID_HANDLE_VALUE)
    #else
    int& P = (int&)Private;
    const int flags = O_WRONLY | O_CREAT | (RejectIfExists ? O_EXCL : 0) | (Truncate ? O_TRUNC : 0);
    const mode_t Mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    P = open(FullName.c_str(), flags, Mode);
    if (P == -1)
    #endif
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
                if (mkdir(t.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
                #endif
                {
                    Private = (void*)-1;
                    return Error_CreateDirectory;
                }
            }
        }
        #if defined(_WIN32) || defined(_WINDOWS)
        P = CreateFileA(FullName.c_str(), GENERIC_WRITE, 0, 0, CreationDisposition, FILE_ATTRIBUTE_NORMAL, 0);
        if (P == INVALID_HANDLE_VALUE)
        #else
        P = open(FullName.c_str(), flags, Mode);
        if (P == -1)
        #endif
        {
            Private = (void*)-1;
            return (access(FullName.c_str(), 0)) ? (RejectIfExists ? Error_FileCreate : Error_FileWrite) : Error_FileAlreadyExists;
        }
    }

    OutputFileName = OutputFileName_Source;
    return OK;
}

//---------------------------------------------------------------------------
// file

file::return_value file::Write(const uint8_t* Buffer, size_t Size)
{
    // Handle size of 0
    if (!Size)
        return OK;

    // Check that a file is open
    size_t Offset = 0;
    #if defined(_WIN32) || defined(_WINDOWS)
    HANDLE& P = (HANDLE&)Private;
    BOOL Result;
    while (Offset < Size)
    {
        DWORD BytesWritten;
        DWORD Size_Temp;
        if (Size - Offset >= (DWORD)-1) // WriteFile() accepts only DWORDs
            Size_Temp = (DWORD)-1;
        else
            Size_Temp = (DWORD)(Size - Offset);
        Result = WriteFile(P, Buffer + Offset, Size_Temp, &BytesWritten, NULL);
        if (BytesWritten == 0 || Result == FALSE)
            break;
        Offset += BytesWritten;
    }
    if (Result == FALSE || Offset < Size)
    #else
    int& P = (int&)Private;
    ssize_t BytesWritten;
    while (Offset < Size)
    {
        size_t Size_Temp = Size - Offset;
        BytesWritten = write(P, Buffer, Size_Temp);
        if (BytesWritten == 0 || BytesWritten == -1)
            break;
        Offset += (size_t)BytesWritten;
    }
    if (BytesWritten == -1 || Offset < Size)
    #endif
    {
        return Error_FileWrite;
    }

    return OK;
}

//---------------------------------------------------------------------------
// file

file::return_value file::Seek(int64_t Offset, seek_value Method)
{
    if (Private == (void*)-1)
        return Error_FileCreate;
        
    #if defined(_WIN32) || defined(_WINDOWS)
    HANDLE& P = (HANDLE&)Private;
    LARGE_INTEGER Offset2;
    Offset2.QuadPart = Offset;
    if (SetFilePointerEx(P, Offset2, NULL, Method) == 0)
    #else
    int& P = (int&)Private;
    int whence;
    switch (Method)
    {
        case Begin   : whence =SEEK_SET; break;
        case Current : whence =SEEK_CUR; break;
        case End     : whence =SEEK_END; break;
        default      : whence =ios_base::beg;
    }
    if (lseek(P, Offset, whence) == (off_t)-1)
    #endif
    {
        return Error_Seek;
    }

    return OK;
}

//---------------------------------------------------------------------------
// file

file::return_value file::SetEndOfFile()
{
    if (Private == (void*)-1)
        return Error_FileCreate;

    #if defined(_WIN32) || defined(_WINDOWS)
    HANDLE& P = (HANDLE&)Private;
    if (!::SetEndOfFile(P))
    #else
    int& P = (int&)Private;
    off_t length = lseek(P, 0, SEEK_CUR);
    if (length == (off_t)-1)
    {
        return Error_FileWrite;
    }
    if (ftruncate(P, length))
    #endif
    {
        return Error_FileWrite;
    }

    return OK;
}

//---------------------------------------------------------------------------
// file

file::return_value file::Close()
{
    if (Private == (void*)-1)
        return OK;
        
    #if defined(_WIN32) || defined(_WINDOWS)
    HANDLE& P = (HANDLE&)Private;
    if (CloseHandle(P) == 0)
    #else
    int& P = (int&)Private;
    if (close(P))
    #endif
    {
        Private = (void*)-1;
        OutputFileName.clear();
        return Error_FileWrite;
    }

    Private = (void*)-1;
    OutputFileName.clear();
    return OK;
}
