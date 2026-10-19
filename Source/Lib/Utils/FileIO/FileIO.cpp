/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE // Needed for ftruncate on GNU compiler
#endif
#include <atomic>
#include <cstring>
#include <iostream>
#include <sstream>
#include <fstream>
#if defined(_WIN32) || defined(_WINDOWS)
    #include "windows.h"
    #include <winternl.h> // NtCreateFile
    #include <stdio.h>
    #include <fcntl.h>
    #include <io.h> // File existence
    #include <direct.h> // Directory creation
    #define access(FileName, Mode) _waccess_s(WideFileName(FileName).c_str(), Mode)
    #define mkdir(FileName) _wmkdir(WideFileName(FileName).c_str())
    #define stat _stat
#else
    #include <dirent.h>
    #include <fcntl.h>
    #include <glob.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/mman.h>
#endif
#include "Lib/Utils/FileIO/FileIO.h"
#if defined(_WIN32) || defined(_WINDOWS)
    #define pclose _pclose
#else
    #define _read read
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WINDOWS)
static wstring WideName(const char* FileName)
{
    // Same code page as the ANSI API
    auto CodePage = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
    auto Size = MultiByteToWideChar(CodePage, 0, FileName, -1, nullptr, 0);
    if (Size <= 1)
        return {};
    wstring Name(Size, L'\0');
    MultiByteToWideChar(CodePage, 0, FileName, -1, &Name[0], Size);
    Name.resize(Size - 1);
    return Name;
}

//---------------------------------------------------------------------------
wstring WideFileName(const char* FileName)
{
    auto Name = WideName(FileName);
    if (Name.empty())
        return {};

    // Already a Win32 file or device namespace name
    if (Name.size() >= 4 && Name[0] == L'\\' && Name[1] == L'\\' && (Name[2] == L'?' || Name[2] == L'.') && Name[3] == L'\\')
        return Name;

    // Full path, the Win32 API would build it (relative path, '/', "..") with the MAX_PATH limitation (MAX_PATH - 12 for directories)
    auto FullSize = GetFullPathNameW(Name.c_str(), 0, nullptr, nullptr);
    if (FullSize < MAX_PATH - 12)
        return Name; // Short enough or not handled (error)
    wstring FullName(FullSize, L'\0');
    FullSize = GetFullPathNameW(Name.c_str(), FullSize, &FullName[0], nullptr);
    if (!FullSize || FullSize >= FullName.size())
        return Name;
    FullName.resize(FullSize);

    // Long path
    if (FullName[0] == L'\\' && FullName[1] == L'\\')
        return L"\\\\?\\UNC" + FullName.substr(1); // UNC path, \\server\share --> \\?\UNC\server\share
    return L"\\\\?\\" + FullName;
}

//---------------------------------------------------------------------------
string NarrowFileName(const wchar_t* FileName)
{
    // Same code page as the ANSI API
    auto CodePage = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
    auto Size = WideCharToMultiByte(CodePage, 0, FileName, -1, nullptr, 0, nullptr, nullptr);
    if (Size <= 1)
        return {};
    string Name(Size, '\0');
    WideCharToMultiByte(CodePage, 0, FileName, -1, &Name[0], Size, nullptr, nullptr);
    Name.resize(Size - 1);
    return Name;
}

//---------------------------------------------------------------------------
// Opens a file from its name relative to a directory, with the same options as CreateFileW() with OPEN_EXISTING and FILE_FLAG_RANDOM_ACCESS
static HANDLE CreateFile_Relative(HANDLE Dir, const char* FileName, DWORD DesiredAccess, DWORD ShareMode)
{
    typedef NTSTATUS (NTAPI* NtCreateFile_Type)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
    static const auto NtCreateFile_Ptr = (NtCreateFile_Type)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtCreateFile"); // Loaded at run time, no need of ntdll.lib
    if (!NtCreateFile_Ptr)
        return INVALID_HANDLE_VALUE;

    auto Name = WideName(FileName);
    auto Name_Size = Name.size() * sizeof(wchar_t);
    if (Name.empty() || Name_Size > 0xFFFE)
        return INVALID_HANDLE_VALUE;
    UNICODE_STRING Name_Unicode;
    Name_Unicode.Length = (USHORT)Name_Size;
    Name_Unicode.MaximumLength = (USHORT)Name_Size;
    Name_Unicode.Buffer = &Name[0];
    OBJECT_ATTRIBUTES Attributes;
    InitializeObjectAttributes(&Attributes, &Name_Unicode, OBJ_CASE_INSENSITIVE, Dir, nullptr);
    IO_STATUS_BLOCK IoStatus;
    HANDLE NewFile;
    const ULONG Disposition_Open = 0x00000001; // FILE_OPEN
    const ULONG Options = 0x00000040 | 0x00000020 | 0x00000800; // FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT (as CreateFileW() without FILE_FLAG_OVERLAPPED) | FILE_RANDOM_ACCESS
    if (NtCreateFile_Ptr(&NewFile, DesiredAccess | SYNCHRONIZE | FILE_READ_ATTRIBUTES, &Attributes, &IoStatus, nullptr, FILE_ATTRIBUTE_NORMAL, ShareMode, Disposition_Open, Options, nullptr, 0) < 0)
        return INVALID_HANDLE_VALUE;
    return NewFile;
}
#endif //defined(_WIN32) || defined(_WINDOWS)

//---------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WINDOWS)
static const char* const PathSeparators = "/\\";
#else //defined(_WIN32) || defined(_WINDOWS)
static const char* const PathSeparators = "/";
#endif //defined(_WIN32) || defined(_WINDOWS)

//---------------------------------------------------------------------------
int directory::Open(const string& Path_)
{
    Close();

    // Name of the directory without the trailing path separator, except for a root directory
    auto Name = Path_;
    if (Name.empty())
        Name = '.';
    else if (Name.size() > 1 && strchr(PathSeparators, Name.back()) && Name[Name.size() - 2] != ':')
        Name.pop_back();

    #if defined(_WIN32) || defined(_WINDOWS)
    auto NewDir = CreateFileW(WideFileName(Name).c_str(), FILE_TRAVERSE | SYNCHRONIZE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (NewDir == INVALID_HANDLE_VALUE)
        return 1;
    #else //defined(_WIN32) || defined(_WINDOWS)
    auto NewDir = open(Name.c_str(), O_RDONLY | O_DIRECTORY);
    if (NewDir == -1)
        return 1;
    #endif //defined(_WIN32) || defined(_WINDOWS)
    Private = NewDir;
    Path = Path_;
    return 0;
}

//---------------------------------------------------------------------------
int directory::Open_FromFileName(const string& FileName)
{
    auto Path_Size = FileName.find_last_of(PathSeparators);
    return Open(Path_Size == string::npos ? string() : FileName.substr(0, Path_Size + 1));
}

//---------------------------------------------------------------------------
void directory::Close()
{
    if (!IsOpen())
        return;
    #if defined(_WIN32) || defined(_WINDOWS)
    CloseHandle(Private);
    #else //defined(_WIN32) || defined(_WINDOWS)
    close(Private);
    #endif //defined(_WIN32) || defined(_WINDOWS)
    Private = (decltype(Private))-1;
    Path.clear();
}

//---------------------------------------------------------------------------
const char* directory::RelativeName(const char* FileName) const
{
    if (!IsOpen() || strncmp(FileName, Path.c_str(), Path.size()))
        return nullptr;
    auto Name = FileName + Path.size();
    if (!*Name || strpbrk(Name, PathSeparators))
        return nullptr; // Not a file name or in a subdirectory
    return Name;
}

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
int filemap::Open_ReadMode(const char* FileName, uintmax_t FileSize, method NewStyle, size_t Begin, size_t End, bool AlsoWrite_, const directory* Dir)
{
    Close();

    AlsoWrite = AlsoWrite_;

    if (NewStyle != method::mmap)
    {
        Method = NewStyle;
        private_buffered* P = new private_buffered;
        P->MaxSize = End - Begin;

        switch (Method)
        {
        default: // case style::fstream:
        {
            auto Flags = ios::binary;
            if (!FileSize)
            {
                Flags |= ios::ate;
            }
            #if defined(_WIN32) || defined(_WINDOWS)
            auto F = new ifstream(WideFileName(FileName).c_str(), Flags);
            #else //defined(_WIN32) || defined(_WINDOWS)
            auto F = new ifstream(FileName, Flags);
            #endif //defined(_WIN32) || defined(_WINDOWS)
            if (!F->is_open())
                return 1;
            auto ShallSeek = !FileSize || Begin;
            if (!FileSize)
            {
                FileSize = F->tellg();
            }
            if (ShallSeek)
            {
                F->seekg(Begin, F->beg);
                if (!F->good())
                    return 1;
            }
            P->F.Ifstream = F;
            break;
        }
        case method::fopen:
        {
            #if defined(_WIN32) || defined(_WINDOWS)
            FILE* F = _wfopen(WideFileName(FileName).c_str(), L"rb");
            #else //defined(_WIN32) || defined(_WINDOWS)
            FILE* F = fopen(FileName, "rb");
            #endif //defined(_WIN32) || defined(_WINDOWS)
            if (!F)
                return 1;
            if (!FileSize)
            {
                #if defined(_WIN32) || defined(_WINDOWS)
                struct _stat64i32 Fstat;
                if (_fstat(_fileno(F), &Fstat))
                #else
                struct stat Fstat;
                if (fstat(fileno(F), &Fstat))
                #endif
                {
                    fclose(F);
                    return 1;
                }
                FileSize = Fstat.st_size;
            }
            P->F.File = F;
            break;
        }
        case method::open:
        {
            #if defined(_WIN32) || defined(_WINDOWS)
            auto F = _wopen(WideFileName(FileName).c_str(), _O_BINARY | _O_RDONLY | _O_SEQUENTIAL, _S_IREAD);
            #else //defined(_WIN32) || defined(_WINDOWS)
            auto F = open(FileName, O_RDONLY);
            #endif //defined(_WIN32) || defined(_WINDOWS)
            if (F == -1)
                return 1;
            if (!FileSize)
            {
                #if defined(_WIN32) || defined(_WINDOWS)
                struct _stat64i32 Fstat;
                if (_fstat(F, &Fstat))
                #else
                struct stat Fstat;
                if (fstat(F, &Fstat))
                #endif
                {
                    #if defined(_WIN32) || defined(_WINDOWS)
                    _close(F);
                    #else
                    close(F);
                    #endif
                    return 1;
                }
                FileSize = Fstat.st_size;
            }
            P->F.Int = F;
            break;
        }
        #if defined(_WIN32) || defined(_WINDOWS)
        case method::createfile:
        {
            auto NewFile = CreateFileW(WideFileName(FileName).c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, 0);
            if (NewFile == INVALID_HANDLE_VALUE)
                return 1;
            if (!FileSize)
            {
                DWORD FileSizeHigh;
                auto FileSizeLow = GetFileSize(NewFile, &FileSizeHigh);
                if ((FileSizeLow != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) // If no error (special case with 32-bit max value)
                    && (!FileSizeHigh || sizeof(size_t) >= 8)) // Mapping 4+ GiB files is not supported in 32-bit mode
                {
                    FileSize = ((size_t)FileSizeHigh) << 32 | FileSizeLow;
                }
                else
                    return 1;
            }
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

    // Open, relative to the directory if possible
    // Random access if a directory is provided (only the beginning of the files is needed): no read-ahead of the rest of the files
    auto FileName_Relative = Dir ? Dir->RelativeName(FileName) : nullptr;
#if defined(_WIN32) || defined(_WINDOWS)
    static const DWORD DesiredAccess[2] = { GENERIC_READ, GENERIC_READ | GENERIC_WRITE };
    static const DWORD ShareMode[2] = { FILE_SHARE_READ, FILE_SHARE_READ | FILE_SHARE_WRITE };
    static const DWORD Flags[2] = { FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, FILE_ATTRIBUTE_NORMAL };
    auto NewFile = FileName_Relative ? CreateFile_Relative(Dir->Private, FileName_Relative, DesiredAccess[AlsoWrite], ShareMode[AlsoWrite]) : INVALID_HANDLE_VALUE;
    if (NewFile == INVALID_HANDLE_VALUE)
        NewFile = CreateFileW(WideFileName(FileName).c_str(), DesiredAccess[AlsoWrite], ShareMode[AlsoWrite], 0, OPEN_EXISTING, Dir ? (FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS) : Flags[AlsoWrite], 0);
    if (NewFile == INVALID_HANDLE_VALUE)
        return 1;
    if (!FileSize)
    {
        DWORD FileSizeHigh;
        auto FileSizeLow = GetFileSize(NewFile, &FileSizeHigh);
        if ((FileSizeLow != INVALID_FILE_SIZE || GetLastError() == NO_ERROR) // If no error (special case with 32-bit max value)
            && (!FileSizeHigh || sizeof(size_t) >= 8)) // Mapping 4+ GiB files is not supported in 32-bit mode
        {
            FileSize = ((size_t)FileSizeHigh) << 32 | FileSizeLow;
        }
        else
            return 1;
    }
    if (FileSize)
    {
        static const DWORD Protects[2] = { PAGE_READONLY, PAGE_READWRITE };
        auto NewMapping = CreateFileMapping(NewFile, 0, Protects[AlsoWrite], 0, 0, 0);
        if (!NewMapping)
        {
            CloseHandle(NewFile);
            return 1;
        }
        Private = NewFile;
        Private2 = NewMapping;
    }
    else
        Private = NewFile; // CreateFileMapping does not support 0-byte files, so we will map manually to nullptr
#else 
    static const int oflag[2] = { O_RDONLY, O_RDWR };
    auto fd = FileName_Relative ? openat(Dir->Private, FileName_Relative, oflag[AlsoWrite], 0) : -1;
    if (fd == -1)
        fd = open(FileName, oflag[AlsoWrite], 0);
    if (fd == -1)
        return 1;
    if (Dir)
    {
        #if defined(POSIX_FADV_RANDOM)
            posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM); // Linux and others
        #elif defined(F_RDAHEAD)
            fcntl(fd, F_RDAHEAD, 0); // macOS
        #endif
    }
    if (!FileSize)
    {
        struct stat Fstat;
        if (fstat(fd, &Fstat))
        {
            close(fd);
            return 1;
        }
        FileSize = Fstat.st_size;
    }
    Private = fd;
#endif

    AssignKeepDataBase(FileSize); // Intermediate, Remap() will set the data pointer
    return Remap();
}

//---------------------------------------------------------------------------
int filemap::Open_HeaderMode(const string& FileName, size_t HeaderSize, bool AlsoWrite_, uintmax_t FileSize, const directory* Dir)
{
    Close();

    AlsoWrite = AlsoWrite_;
    Method = method::header;

    // Open, relative to the directory if possible
    // Random access: no read-ahead of the content after the header
    auto FileName_Relative = Dir ? Dir->RelativeName(FileName.c_str()) : nullptr;
#if defined(_WIN32) || defined(_WINDOWS)
    static const DWORD DesiredAccess[2] = { GENERIC_READ, GENERIC_READ | GENERIC_WRITE };
    static const DWORD ShareMode[2] = { FILE_SHARE_READ, FILE_SHARE_READ | FILE_SHARE_WRITE };
    auto NewFile = FileName_Relative ? CreateFile_Relative(Dir->Private, FileName_Relative, DesiredAccess[AlsoWrite], ShareMode[AlsoWrite]) : INVALID_HANDLE_VALUE;
    if (NewFile == INVALID_HANDLE_VALUE)
        NewFile = CreateFileW(WideFileName(FileName).c_str(), DesiredAccess[AlsoWrite], ShareMode[AlsoWrite], 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, 0);
    if (NewFile == INVALID_HANDLE_VALUE)
    {
        Close();
        return 1;
    }
    Private = NewFile;
    if (!FileSize)
    {
        LARGE_INTEGER FileSize_Temp;
        if (!GetFileSizeEx(NewFile, &FileSize_Temp))
        {
            Close();
            return 1;
        }
        FileSize = FileSize_Temp.QuadPart;
    }
#else
    static const int oflag[2] = { O_RDONLY, O_RDWR };
    auto fd = FileName_Relative ? openat(Dir->Private, FileName_Relative, oflag[AlsoWrite], 0) : -1;
    if (fd == -1)
        fd = open(FileName.c_str(), oflag[AlsoWrite], 0);
    if (fd == -1)
    {
        Close();
        return 1;
    }
    Private = fd;
    #if defined(POSIX_FADV_RANDOM)
        posix_fadvise(fd, 0, 0, POSIX_FADV_RANDOM); // Linux and others
    #elif defined(F_RDAHEAD)
        fcntl(fd, F_RDAHEAD, 0); // macOS
    #endif
    if (!FileSize)
    {
        struct stat Fstat;
        if (fstat(fd, &Fstat))
        {
            Close();
            return 1;
        }
        FileSize = Fstat.st_size;
    }
#endif
    if (sizeof(size_t) < 8 && FileSize >> 32) // Files of 4+ GiB are not supported in 32-bit mode
    {
        Close();
        return 1;
    }

    // Read
    if (HeaderSize > FileSize)
        HeaderSize = (size_t)FileSize;
    if (!HeaderSize)
        return 0; // Special case for 0-byte files
    if (HeaderSize > Header_Buffer_Size) // Reuse of the buffer of the previous file if big enough
    {
        delete[] Header_Buffer;
        Header_Buffer = new uint8_t[HeaderSize];
        Header_Buffer_Size = HeaderSize;
    }
    auto Header = Header_Buffer;
    AssignBase(Header, (size_t)FileSize);
#if defined(_WIN32) || defined(_WINDOWS)
    DWORD BytesRead;
    if (HeaderSize > (DWORD)-1 || !ReadFile(Private, Header, (DWORD)HeaderSize, &BytesRead, nullptr))
    {
        Close();
        return 1;
    }
    Header_Size = BytesRead;
#else
    while (Header_Size < HeaderSize)
    {
        auto BytesRead = read(Private, Header + Header_Size, HeaderSize - Header_Size);
        if (BytesRead <= 0)
            break;
        Header_Size += BytesRead;
    }
#endif

    return 0;
}

//---------------------------------------------------------------------------
int filemap::Write(size_t Begin, size_t End)
{
    // Memory map, data is already in the file
    if (Method != method::header)
        return AlsoWrite && Method == method::mmap ? 0 : 1;

    if (!AlsoWrite || Begin > End || End > Header_Size)
        return 1;
    auto Size = End - Begin;
#if defined(_WIN32) || defined(_WINDOWS)
    // Memory map of the beginning of the file, quicker than WriteFile() when the file is closed just after (antivirus software
    // e.g. Windows Defender scans the file when it is closed after a WriteFile(), not after a write in a memory map)
    auto Mapping = CreateFileMapping(Private, 0, PAGE_READWRITE, 0, 0, 0);
    if (!Mapping)
        return 1;
    auto View = (uint8_t*)MapViewOfFile(Mapping, FILE_MAP_WRITE, 0, 0, End);
    if (!View)
    {
        CloseHandle(Mapping);
        return 1;
    }
    memcpy(View + Begin, Data() + Begin, Size);
    UnmapViewOfFile(View);
    CloseHandle(Mapping);
#else
    if (pwrite(Private, Data() + Begin, Size, Begin) != (ssize_t)Size)
        return 1;
#endif
    return 0;
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

//---------------------------------------------------------------------------
// Memory map, unmapped when the file map and the users who held it released it
struct filemap_map
{
    const uint8_t*              Data;
    size_t                      Size;
    std::atomic<size_t>         Count; // File map and holders
};

//---------------------------------------------------------------------------
void* filemap::Hold()
{
    if (!Map)
        return nullptr;
    ((filemap_map*)Map)->Count++;
    return Map;
}

//---------------------------------------------------------------------------
void filemap::Release(void* Map_)
{
    auto Map = (filemap_map*)Map_;
    if (--Map->Count)
        return;
#if defined(_WIN32) || defined(_WINDOWS)
    UnmapViewOfFile(Map->Data);
#else
    munmap_const(Map->Data, Map->Size);
#endif
    delete Map;
}

//---------------------------------------------------------------------------
size_t filemap::Readable_End()
{
    if (Method == method::header)
        return Header_Size;
    if (Method != method::mmap)
    {
        auto P = (private_buffered*)Private2;
        return P->Data_Shift + P->MaxSize;
    }
    return Size();
}
int filemap::Remap(size_t Begin, size_t End)
{
    // Special case for 0-byte files
    if (Empty())
        return 0;

    if (Method == method::header)
        return 1; // Not supported

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
        if (Buffer_Middle_Size < Buffer_MaxSize)
        {
            memmove(const_cast<uint8_t*>(Buffer), const_cast<uint8_t*>(Buffer_Middle), Buffer_Middle_Size);
        }
        P->Data_Shift += Begin;
        AssignKeepSizeBase(Buffer - P->Data_Shift);
        Buffer += Buffer_Middle_Size;
        Buffer_MaxSize -= Buffer_Middle_Size;

        switch (Method)
        {
        default: // case style::fstream:
        {
            auto F = P->F.Ifstream;
            F->read((char*)const_cast<uint8_t*>(Buffer), Buffer_MaxSize);
            break;
        }
        case method::fopen:
        {
            auto F = P->F.File;
            if (fread((char*)const_cast<uint8_t*>(Buffer), 1, Buffer_MaxSize, F) != Buffer_MaxSize)
                return 1;
            break;
        }
        case method::open:
        {
            auto F = P->F.Int;
            if (Buffer_MaxSize >= (unsigned int)-1)
                return 1;
            if (_read(F, const_cast<uint8_t*>(Buffer), (unsigned int)Buffer_MaxSize) == -1)
                return 1;
            break;
        }
        #if defined(_WIN32) || defined(_WINDOWS)
        case method::createfile:
        {
            auto F = P->F.Handle;
            DWORD BytesRead = 0;
            if (!ReadFile(F, (LPVOID)Buffer, (DWORD)Buffer_MaxSize, &BytesRead, nullptr) || BytesRead == 0)
                return 1;
            break;
        }
        #endif //defined(_WIN32) || defined(_WINDOWS)
        }

        return 0;
    }

    // Close previous map, kept by its users if any
    if (Data())
    {
        Release(Map);
        Map = nullptr;
        ClearKeepSizeBase(); // Intermediate, size is needed later
    }

    // New map
#if defined(_WIN32) || defined(_WINDOWS)
    static const DWORD DesiredAccess[2] = { FILE_MAP_READ, FILE_MAP_READ | FILE_MAP_WRITE };
    auto NewData = MapViewOfFile(Private2, DesiredAccess[AlsoWrite], 0, 0, 0);
    const decltype(NewData) NewData_Fail = NULL;
#else
    static const int prot[2] = { PROT_READ, PROT_READ | PROT_WRITE };
    static const int flags[2] = { MAP_PRIVATE, MAP_SHARED };
    auto NewData = mmap(nullptr, Size(), prot[AlsoWrite], flags[AlsoWrite], Private, 0);
    const decltype(NewData) NewData_Fail = MAP_FAILED;
#endif

    // Assign
    if (NewData == NewData_Fail)
    {
        Close();
        return 1;
    }
    AssignKeepSizeBase((const uint8_t*)NewData);
    Map = new filemap_map{ (const uint8_t*)NewData, Size(), { 1 } };
    return 0;
}

//---------------------------------------------------------------------------
int filemap::Close()
{
    // Close header copy, buffer kept for the next file
    if (Method == method::header)
    {
        ClearBase();
        Header_Size = 0;
        Method = {};
    }

    // Close map, kept by its users if any
    else if (Map)
    {
        Release(Map);
        Map = nullptr;
        ClearBase();
    }
    else if (Data())
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
    P = CreateFileW(WideFileName(FullName).c_str(), GENERIC_WRITE, 0, 0, CreationDisposition, FILE_ATTRIBUTE_NORMAL, 0);
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
                if (mkdir(t.c_str()) && access(t.c_str(), 0)) // Directory may be created by another thread in the meantime
                #else
                if (mkdir(t.c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH) && access(t.c_str(), 0)) // Directory may be created by another thread in the meantime
                #endif
                {
                    Private = (void*)-1;
                    return Error_CreateDirectory;
                }
            }
        }
#if defined(_WIN32) || defined(_WINDOWS)
        P = CreateFileW(WideFileName(FullName).c_str(), GENERIC_WRITE, 0, 0, CreationDisposition, FILE_ATTRIBUTE_NORMAL, 0);
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
