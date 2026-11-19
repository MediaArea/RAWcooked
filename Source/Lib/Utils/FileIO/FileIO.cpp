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
#include <cstdlib>
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
    #include <malloc.h> // _aligned_malloc
    #define access(FileName, Mode) _waccess_s(WideFileName(FileName).c_str(), Mode)
    #define mkdir(FileName) _wmkdir(WideFileName(FileName).c_str())
    #define stat _stat
#else
    #include <cerrno>
    #include <dirent.h>
    #include <fcntl.h>
    #include <glob.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/mman.h>
    #if defined(__APPLE__)
        #include <mach/mach.h> // Available memory
    #endif
#endif
#include "Lib/Utils/FileIO/FileIO.h"
#if defined(_WIN32) || defined(_WINDOWS)
    #define pclose _pclose
#else
    #define _read read
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
static bool IsUTF8(const char* Name, size_t Size)
{
    auto Current = (const uint8_t*)Name;
    auto End = Current + Size;
    while (Current < End)
    {
        auto Value = (uint32_t)*Current++;
        if (Value < 0x80)
            continue;
        size_t Count;
        if ((Value & 0xE0) == 0xC0)
        {
            Count = 1;
            Value &= 0x1F;
        }
        else if ((Value & 0xF0) == 0xE0)
        {
            Count = 2;
            Value &= 0x0F;
        }
        else if ((Value & 0xF8) == 0xF0)
        {
            Count = 3;
            Value &= 0x07;
        }
        else
            return false;
        if ((size_t)(End - Current) < Count)
            return false;
        for (size_t i = 0; i < Count; i++)
        {
            auto Continuation = *Current++;
            if ((Continuation & 0xC0) != 0x80)
                return false;
            Value = (Value << 6) | (Continuation & 0x3F);
        }
        static const uint32_t Minimum[] = { 0x80, 0x800, 0x10000 };
        if (Value < Minimum[Count - 1] || Value > 0x10FFFF || (Value >= 0xD800 && Value <= 0xDFFF))
            return false; // Overlong, out of range or surrogate
    }
    return true;
}

//---------------------------------------------------------------------------
#if defined(_WIN32) || defined(_WINDOWS)
// Code page of the ANSI API, UTF-8 with the activeCodePage of the manifest (Windows 10 version 1903 and later)
static UINT FileApisCodePage()
{
    return AreFileApisANSI() ? GetACP() : GetOEMCP();
}

//---------------------------------------------------------------------------
// ANSI code page of the system, code page of the file names in the files created by the versions without the activeCodePage of the manifest
static UINT LegacyCodePage()
{
    static const UINT CodePage = []() -> UINT
    {
        DWORD Value = 0;
        if (!GetLocaleInfoEx(LOCALE_NAME_SYSTEM_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER, (LPWSTR)&Value, sizeof(Value) / sizeof(WCHAR)) || !Value || Value == CP_UTF8)
            return 1252;
        return (UINT)Value;
    }();
    return CodePage;
}

//---------------------------------------------------------------------------
static wstring ToWide(UINT CodePage, const char* Name, size_t Size)
{
    auto WideSize = MultiByteToWideChar(CodePage, 0, Name, (int)Size, nullptr, 0);
    if (WideSize <= 0)
        return {};
    wstring Result(WideSize, L'\0');
    MultiByteToWideChar(CodePage, 0, Name, (int)Size, &Result[0], WideSize);
    return Result;
}

//---------------------------------------------------------------------------
static string FromWide(UINT CodePage, const wstring& Name)
{
    auto Size = WideCharToMultiByte(CodePage, 0, Name.c_str(), (int)Name.size(), nullptr, 0, nullptr, nullptr);
    if (Size <= 0)
        return {};
    string Result(Size, '\0');
    WideCharToMultiByte(CodePage, 0, Name.c_str(), (int)Name.size(), &Result[0], Size, nullptr, nullptr);
    return Result;
}

//---------------------------------------------------------------------------
static wstring WideName(const char* FileName)
{
    // Same code page as the ANSI API
    auto CodePage = FileApisCodePage();
    auto Size = strlen(FileName);
    if (CodePage == CP_UTF8 && !IsUTF8(FileName, Size))
        CodePage = LegacyCodePage(); // Not UTF-8, e.g. a name from a file created by a previous version, in the ANSI code page
    return ToWide(CodePage, FileName, Size);
}
#else
//---------------------------------------------------------------------------
static string Windows1252ToUTF8(const string& Name)
{
    static const uint16_t Table_80_9F[32] =
    {
        0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x008D, 0x017D, 0x008F,
        0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
    };
    string Result;
    Result.reserve(Name.size() * 2);
    for (auto Char : Name)
    {
        uint32_t Value = (uint8_t)Char;
        if (Value >= 0x80 && Value < 0xA0)
            Value = Table_80_9F[Value - 0x80];
        if (Value < 0x80)
            Result += (char)Value;
        else if (Value < 0x800)
        {
            Result += (char)(0xC0 | (Value >> 6));
            Result += (char)(0x80 | (Value & 0x3F));
        }
        else
        {
            Result += (char)(0xE0 | (Value >> 12));
            Result += (char)(0x80 | ((Value >> 6) & 0x3F));
            Result += (char)(0x80 | (Value & 0x3F));
        }
    }
    return Result;
}
#endif //defined(_WIN32) || defined(_WINDOWS)

//---------------------------------------------------------------------------
bool FileName_LegacyToUTF8(string& Name)
{
    if (IsUTF8(Name.c_str(), Name.size()))
        return false;
    #if defined(_WIN32) || defined(_WINDOWS)
        Name = FromWide(CP_UTF8, ToWide(LegacyCodePage(), Name.c_str(), Name.size()));
    #else
        Name = Windows1252ToUTF8(Name);
    #endif
    return true;
}

//---------------------------------------------------------------------------
void FileName_FromStored(string& Name)
{
    #if defined(_WIN32) || defined(_WINDOWS)
        if (FileApisCodePage() == CP_UTF8) // Else same code page as the previous versions
            FileName_LegacyToUTF8(Name);
    #elif defined(__APPLE__)
        FileName_LegacyToUTF8(Name); // Only UTF-8 in the file systems
    #else
        (void)Name; // Any byte sequence is accepted by the file systems, kept as is
    #endif
}

//---------------------------------------------------------------------------
void FileName_FromUTF8(string& Name)
{
    #if defined(_WIN32) || defined(_WINDOWS)
        auto CodePage = FileApisCodePage();
        if (CodePage != CP_UTF8 && IsUTF8(Name.c_str(), Name.size()))
            Name = FromWide(CodePage, ToWide(CP_UTF8, Name.c_str(), Name.size()));
    #else
        (void)Name;
    #endif
}

#if defined(_WIN32) || defined(_WINDOWS)

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
        {
            CloseHandle(NewFile);
            return 1;
        }
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
// Available physical memory, 0 if unknown
static uint64_t AvailableMemory()
{
    #if defined(_WIN32) || defined(_WINDOWS)
        MEMORYSTATUSEX Status;
        Status.dwLength = sizeof(Status);
        if (GlobalMemoryStatusEx(&Status))
            return Status.ullAvailPhys;
    #elif defined(__APPLE__)
        vm_statistics64_data_t Stats;
        mach_msg_type_number_t Count = HOST_VM_INFO64_COUNT;
        vm_size_t PageSize;
        if (host_page_size(mach_host_self(), &PageSize) == KERN_SUCCESS && host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&Stats, &Count) == KERN_SUCCESS)
            return ((uint64_t)Stats.free_count + Stats.inactive_count + Stats.purgeable_count) * PageSize; // Inactive and purgeable pages can be reused
    #else
        // MemAvailable includes the file cache which can be freed (Linux)
        if (auto File = fopen("/proc/meminfo", "r"))
        {
            char Line[256];
            unsigned long long Value = 0;
            while (fgets(Line, sizeof(Line), File))
                if (sscanf(Line, "MemAvailable: %llu kB", &Value) == 1)
                    break;
            fclose(File);
            if (Value)
                return (uint64_t)Value * 1024;
        }
        #if defined(_SC_AVPHYS_PAGES) && defined(_SC_PAGESIZE)
            auto Pages = sysconf(_SC_AVPHYS_PAGES);
            auto PageSize = sysconf(_SC_PAGESIZE);
            if (Pages > 0 && PageSize > 0)
                return (uint64_t)Pages * PageSize;
        #endif
    #endif
    return 0;
}

//---------------------------------------------------------------------------
size_t filemap::WholeFile_Max = 128 * 1024 * 1024; // Default if the available memory is unknown

//---------------------------------------------------------------------------
void filemap::WholeFile_Max_Set(size_t ThreadCount)
{
    // Up to a quarter of the available memory for the buffers of all threads (bigger files are memory mapped, without committed memory)
    // Up to 1 GiB per file (single read call)
    auto Memory = AvailableMemory();
    if (!Memory || !ThreadCount)
        return;
    auto Max = Memory / 4 / ThreadCount;
    const uint64_t Max_Max = 1024 * 1024 * 1024;
    if (Max > Max_Max)
        Max = Max_Max;
    WholeFile_Max = (size_t)Max;
}

//---------------------------------------------------------------------------
int filemap::Open_HeaderMode(const string& FileName, size_t HeaderSize, bool AlsoWrite_, uintmax_t FileSize, const directory* Dir)
{
    Close();

    AlsoWrite = AlsoWrite_;
    Method = method::header;
    auto IsWholeFile = HeaderSize == WholeFile;

    // Open, relative to the directory if possible
    // Header: random access, no read-ahead of the content after the header
    // Whole file: sequential access, without the file cache on Windows (memory map and file cache cost a lot of CPU when the whole file is read once)
    auto FileName_Relative = Dir && !IsWholeFile ? Dir->RelativeName(FileName.c_str()) : nullptr;
    static const size_t SectorSize = 4096; // Alignment needed without the file cache, 4096 is a multiple of all usual sector sizes
#if defined(_WIN32) || defined(_WINDOWS)
    static const DWORD DesiredAccess[2] = { GENERIC_READ, GENERIC_READ | GENERIC_WRITE };
    static const DWORD ShareMode[2] = { FILE_SHARE_READ, FILE_SHARE_READ | FILE_SHARE_WRITE };
    auto NewFile = FileName_Relative ? CreateFile_Relative(Dir->Private, FileName_Relative, DesiredAccess[AlsoWrite], ShareMode[AlsoWrite]) : INVALID_HANDLE_VALUE;
    if (NewFile == INVALID_HANDLE_VALUE)
        NewFile = CreateFileW(WideFileName(FileName).c_str(), DesiredAccess[AlsoWrite], ShareMode[AlsoWrite], 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | (IsWholeFile ? (FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN) : FILE_FLAG_RANDOM_ACCESS), 0);
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
    #if defined(O_DIRECT)
        if (fd == -1 && IsWholeFile)
            fd = open(FileName.c_str(), oflag[AlsoWrite] | O_DIRECT, 0); // Linux and others, may be not supported by the file system
    #endif
    if (fd == -1)
        fd = open(FileName.c_str(), oflag[AlsoWrite], 0);
    if (fd == -1)
    {
        Close();
        return 1;
    }
    Private = fd;
    if (IsWholeFile)
    {
        #if defined(F_NOCACHE)
            fcntl(fd, F_NOCACHE, 1); // macOS
        #elif defined(POSIX_FADV_SEQUENTIAL)
            posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL); // Without O_DIRECT
        #endif
    }
    else
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

    // Whole file: memory map if the file is big (memory usage)
    if (IsWholeFile && FileSize > WholeFile_Max)
    {
        Close();
        return Open_ReadMode(FileName.c_str(), FileSize, {}, {}, {}, AlsoWrite_);
    }

    // Read
    if (HeaderSize > FileSize)
        HeaderSize = (size_t)FileSize;
    if (!HeaderSize)
        return 0; // Special case for 0-byte files
    auto ReadSize = HeaderSize;
    if (IsWholeFile)
        ReadSize = (ReadSize + SectorSize - 1) / SectorSize * SectorSize; // Without the file cache, the size is a multiple of the sector size
    if (ReadSize > Header_Buffer_Size) // Reuse of the buffer of the previous file if big enough
    {
        Header_Buffer_Free();
        #if defined(_WIN32) || defined(_WINDOWS)
            Header_Buffer = (uint8_t*)_aligned_malloc(ReadSize, SectorSize);
        #else //defined(_WIN32) || defined(_WINDOWS)
            void* Temp;
            Header_Buffer = posix_memalign(&Temp, SectorSize, ReadSize) ? nullptr : (uint8_t*)Temp;
        #endif //defined(_WIN32) || defined(_WINDOWS)
        if (!Header_Buffer)
        {
            Close();
            return 1;
        }
        Header_Buffer_Size = ReadSize;
    }
    auto Header = Header_Buffer;
#if defined(_WIN32) || defined(_WINDOWS)
    DWORD BytesRead;
    if (ReadSize > (DWORD)-1 || !ReadFile(Private, Header, (DWORD)ReadSize, &BytesRead, nullptr))
    {
        Close();
        if (IsWholeFile)
            return Open_ReadMode(FileName.c_str(), FileSize, {}, {}, {}, AlsoWrite_); // E.g. file system not supporting reads without the file cache
        return 1;
    }
    Header_Size = BytesRead;
#else
    while (Header_Size < HeaderSize) // Up to the needed bytes, no additional call for detecting the end of the file
    {
        auto BytesRead = read(Private, Header + Header_Size, ReadSize - Header_Size); // Size rounded up as needed without the file cache
        if (BytesRead <= 0)
            break;
        Header_Size += BytesRead;
    }
    if (IsWholeFile && Header_Size < HeaderSize)
    {
        Close();
        return Open_ReadMode(FileName.c_str(), FileSize, {}, {}, {}, AlsoWrite_); // E.g. file system not supporting reads without the file cache
    }
#endif
    if (Header_Size > HeaderSize)
        Header_Size = HeaderSize;
    AssignBase(Header, IsWholeFile ? Header_Size : (size_t)FileSize);

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

//---------------------------------------------------------------------------
bool filemap::Read(size_t Offset, uint8_t* Buffer, size_t Read_Size)
{
    if (Method != method::mmap || Private == (decltype(Private))-1 || Offset > Size() || Read_Size > Size() - Offset)
        return true;
    while (Read_Size)
    {
#if defined(_WIN32) || defined(_WINDOWS)
        OVERLAPPED Overlapped = {};
        Overlapped.Offset = (DWORD)Offset;
        Overlapped.OffsetHigh = (DWORD)((uint64_t)Offset >> 32);
        DWORD BytesRead = 0;
        if (!ReadFile(Private, Buffer, Read_Size < 0x40000000 ? (DWORD)Read_Size : 0x40000000, &BytesRead, &Overlapped) || !BytesRead)
            return true;
#else
        auto BytesRead = pread(Private, Buffer, Read_Size, (off_t)Offset);
        if (BytesRead <= 0)
        {
            if (BytesRead && errno == EINTR)
                continue;
            return true;
        }
#endif
        Offset += (size_t)BytesRead;
        Buffer += (size_t)BytesRead;
        Read_Size -= (size_t)BytesRead;
    }
    return false;
}

//---------------------------------------------------------------------------
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
void filemap::Header_Buffer_Free()
{
    #if defined(_WIN32) || defined(_WINDOWS)
        _aligned_free(Header_Buffer);
    #else //defined(_WIN32) || defined(_WINDOWS)
        free(Header_Buffer);
    #endif //defined(_WIN32) || defined(_WINDOWS)
    Header_Buffer = nullptr;
    Header_Buffer_Size = 0;
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
        #if defined(_WIN32) || defined(_WINDOWS)
        // Root of UNC and Win32 namespace paths (\\server\share, \\?\C:, \\?\UNC\server\share) is not a directory to create, the creation starts after it
        auto IsSeparator = [&](size_t Pos) { return FullName[Pos] == '\\' || FullName[Pos] == '/'; };
        if (FullName.size() > 4 && IsSeparator(0) && IsSeparator(1))
        {
            size_t Root_Count;
            if ((FullName[2] == '?' || FullName[2] == '.') && IsSeparator(3))
            {
                i = 3;
                Root_Count = (FullName.size() > 7 && !_strnicmp(FullName.c_str() + 4, "UNC", 3) && IsSeparator(7)) ? 3 : 1; // \\?\UNC\server\share or \\?\C:
            }
            else
            {
                i = 1;
                Root_Count = 2; // \\server\share
            }
            while (Root_Count-- && i != (size_t)-1)
                i = FullName.find_first_of("/\\", i + 1);
            if (i == (size_t)-1)
                i = FullName.size(); // Only the root, nothing to create
        }
        #endif
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
        BytesWritten = write(P, Data + Offset, Size_Temp);
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
