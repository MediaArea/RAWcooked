/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE // Needed for ftruncate on GNU compiler
#endif
#include "Lib/Utils/FileIO/FileIO.h"
#include <iostream>
#include <sstream>
#include <fstream>
#if defined(_WIN32) || defined(_WINDOWS)
    #include "windows.h"
    #include <stdio.h>
    #include <fcntl.h>
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
struct private_buffered
{
    union f
    {
        FILE*           File;
        ifstream*       Ifstream;
        int             Int;
        #if defined(_WIN32) || defined(_WINDOWS)
        HANDLE          Handle;
        #endif
    };
    f                   F;
    size_t              Data_Shift = 0;
    size_t              MaxSize = 0;
};

//---------------------------------------------------------------------------
int filemap::Open_ReadMode(const char* FileName, method NewStyle, size_t Begin, size_t End)
{
    Close();

    if (NewStyle != method::mmap)
    {
        Method = NewStyle;
        private_buffered* P = new private_buffered;
        P->MaxSize = End - Begin;
        size_t FileSize;

        switch (Method)
        {
        default: // case style::fstream:
        {
            auto F = new ifstream(FileName, ios::binary);
            F->seekg(0, F->end);
            FileSize = F->tellg();
            F->seekg(Begin, F->beg);
            P->F.Ifstream = F;
            break;
        }
        case method::fopen:
        {
            struct stat Fstat;
            if (stat(FileName, &Fstat))
                return 1;
            FileSize = Fstat.st_size;
            auto F = fopen(FileName, "rb");
            P->F.File = F;
            break;
        }
        case method::open:
        {
            struct stat Fstat;
            if (stat(FileName, &Fstat))
                return 1;
            FileSize = Fstat.st_size;
            #if defined(_WIN32) || defined(_WINDOWS)
            auto F = _open(FileName, _O_BINARY | _O_RDONLY | _O_SEQUENTIAL, _S_IREAD);
            #else //defined(_WIN32) || defined(_WINDOWS)
            auto F = open(FileName, O_RDONLY);
            #endif //defined(_WIN32) || defined(_WINDOWS)
            if (F == -1)
                return 1;
            P->F.Int = F;
            break;
        }
        #if defined(_WIN32) || defined(_WINDOWS)
        case method::createfile:
        {
            DWORD FileSizeHigh;
            auto NewFile = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
            auto FileSizeLow = GetFileSize(NewFile, &FileSizeHigh);
            if ((FileSizeLow != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) // If no error (special case with 32-bit max value)
                && (!FileSizeHigh || sizeof(size_t) >= 8)) // Mapping 4+ GiB files is not supported in 32-bit mode
            {
                FileSize = ((size_t)FileSizeHigh) << 32 | FileSizeLow;
            }
            else
                return 1;
            if (Begin)
            {
                LARGE_INTEGER GoTo;
                GoTo.QuadPart = Begin;
                if (!SetFilePointerEx(NewFile, GoTo, nullptr, 0))
                    return 1;
                P->Data_Shift = Begin;
            }
            P->F.Handle = NewFile;
            break;
        }
        #endif //defined(_WIN32) || defined(_WINDOWS)
        }

        auto Buffer = new uint8_t[P->MaxSize];
        P->Data_Shift -= P->MaxSize;
        AssignBase(Buffer - P->Data_Shift, FileSize);
        Private2 = (decltype(Private2))P;

        return Remap(Begin, End);
    }

    size_t NewSize;
#if defined(_WIN32) || defined(_WINDOWS)
    auto NewFile = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (NewFile != INVALID_HANDLE_VALUE)
    {
        DWORD FileSizeHigh;
        auto FileSizeLow = GetFileSize(NewFile, &FileSizeHigh);
        if ((FileSizeLow != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) // If no error (special case with 32-bit max value)
            && (!FileSizeHigh || sizeof(size_t) >= 8)) // Mapping 4+ GiB files is not supported in 32-bit mode
        {
            NewSize = ((size_t)FileSizeHigh) << 32 | FileSizeLow;
            if (NewSize)
            {
                auto NewMapping = CreateFileMapping(NewFile, 0, PAGE_READONLY, 0, 0, 0);
                if (NewMapping)
                {
                    Private = NewFile;
                    Private2 = NewMapping;
                }
                else
                    CloseHandle(NewFile);
            }
            else
                Private = NewFile; // CreateFileMapping does not support 0-byte files, so we will map manually to nullptr
        }
        else
        {
            CloseHandle(NewFile);
        }
    }
#else
    auto fd = open(FileName, O_RDONLY, 0);
    if (fd != -1)
    {
        struct stat Fstat;
        if (!stat(FileName, &Fstat))
        {
            NewSize = Fstat.st_size;
            Private = fd;
        }
        else
            close(fd);
    }
#endif

    if (Private == (decltype(Private))-1)
        return 1;
    AssignKeepDataBase(NewSize); // Intermediate, Remap() will set the data pointer
    return Remap();
}

//---------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WINDOWS)
#else
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
inline int munmap_const(const void* addr, size_t length)
{
    return munmap((void*)addr, length);
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#endif
int filemap::Remap(size_t Begin, size_t End)
{
    // Special case for 0-byte files
    if (Empty())
        return 0;

    if (Method != method::mmap)
    {
        auto P = (private_buffered*)Private2;
        auto Buffer = Data() + P->Data_Shift;
        auto Buffer_MaxSize = P->MaxSize;
        Begin -= P->Data_Shift;
        if (!End)
            End = Size();
        End -= P->Data_Shift;
        auto Buffer_Middle = Buffer + Begin;
        auto Buffer_Middle_Size = Buffer_MaxSize - Begin;
        memmove((void*)Buffer, (void*)Buffer_Middle, Buffer_Middle_Size);
        P->Data_Shift += Begin;
        AssignKeepSizeBase(Buffer - P->Data_Shift);
        Buffer += Buffer_Middle_Size;
        Buffer_MaxSize -= Buffer_Middle_Size;

        switch (Method)
        {
        default: // case style::fstream:
        {
            auto F = P->F.Ifstream;
            F->read((char*)Buffer, Buffer_MaxSize);
            break;
        }
        case method::fopen:
        {
            auto F = P->F.File;
            if (fread((char*)Buffer, Buffer_MaxSize, 1, F) != 1)
                return 1;
            break;
        }
        case method::open:
        {
            auto F = P->F.Int;
            read(F, (void*)Buffer, Buffer_MaxSize);
            break;
        }
        #if defined(_WIN32) || defined(_WINDOWS)
        case method::createfile:
        {
            auto F = P->F.Handle;
            ReadFile(F, (LPVOID)Buffer, (DWORD)Buffer_MaxSize, nullptr, 0);
            break;
        }
        #endif //defined(_WIN32) || defined(_WINDOWS)
        }

        return 0;
    }

    // Close previous map
    if (Data())
    {
#if defined(_WIN32) || defined(_WINDOWS)
        UnmapViewOfFile(Data());
#else
        munmap_const(Data(), Size());
#endif
        ClearKeepSizeBase(); // Intermediate, size is needed later
    }

    // New map
#if defined(_WIN32) || defined(_WINDOWS)
    auto NewData = MapViewOfFile(Private2, FILE_MAP_READ, 0, 0, 0);
    const decltype(NewData) NewData_Fail = NULL;
#else
    auto NewData = mmap(nullptr, Size(), PROT_READ, MAP_FILE | MAP_PRIVATE, Private, 0);
    const decltype(NewData) NewData_Fail = MAP_FAILED;
#endif

    // Assign
    if (NewData == NewData_Fail)
    {
        Close();
        return 1;
    }
    AssignKeepSizeBase((const uint8_t*)NewData);
    return 0;
}

//---------------------------------------------------------------------------
int filemap::Close()
{
    // Close map
    if (Data())
    {
#if defined(_WIN32) || defined(_WINDOWS)
            UnmapViewOfFile(Data());
#else
            munmap_const(Data(), Size());
#endif
        ClearBase();
    }

    // Close intermediate
#if defined(_WIN32) || defined(_WINDOWS)
    if (Private2 != (decltype(Private2))-1)
    {
        CloseHandle(Private2);
        Private2 = (decltype(Private))-1;
    }
#endif

    // Close file
    if (Private != (decltype(Private))-1)
    {
#if defined(_WIN32) || defined(_WINDOWS)
            if (CloseHandle(Private) == NULL)
            {
                Private = (decltype(Private))-1;
                return 1;
            }
#else
            if (close(Private))
            {
                Private = (decltype(Private))-1;
                return 1;
            }
#endif
        Private = (decltype(Private))-1;;
    }

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

file::return_value file::Write(const uint8_t* Data, size_t Size)
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
        Result = WriteFile(P, Data + Offset, Size_Temp, &BytesWritten, nullptr);
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
        BytesWritten = write(P, Data, Size_Temp);
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
    if (SetFilePointerEx(P, Offset2, nullptr, Method) == 0)
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
