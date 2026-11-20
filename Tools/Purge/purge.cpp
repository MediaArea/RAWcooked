/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

// Empties the file cache of the system, for benchmarking as if files were read for the first time
// - Windows: flush of the volume, then as RAMMap "Empty System Working Set", "Empty Modified Page List" and "Empty Standby List", needs to run as administrator
// - Linux: sync then drop_caches, needs to run as root (sudo)
// - macOS: sync then the purge command of the system, needs to run as root (sudo)
// Usage: purge [volume letter of a local volume, default C, Windows only]
// Build:
// - Windows (Visual Studio native tools command prompt of the architecture of the system, e.g. x64): cl /nologo /O2 /EHsc purge.cpp advapi32.lib
// - Linux and macOS: c++ -O2 -o purge purge.cpp
// Linux and macOS versions are not tested
#include <stdio.h>

#if defined(_WIN32) || defined(_WINDOWS)
#include <windows.h>

typedef LONG (NTAPI* NtSetSystemInformation_Type)(INT, PVOID, ULONG);

static bool EnablePrivilege(const wchar_t* Name)
{
    HANDLE Token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &Token))
        return false;
    TOKEN_PRIVILEGES Privileges = {};
    Privileges.PrivilegeCount = 1;
    Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    auto Result = LookupPrivilegeValueW(nullptr, Name, &Privileges.Privileges[0].Luid)
        && AdjustTokenPrivileges(Token, FALSE, &Privileges, 0, nullptr, nullptr)
        && GetLastError() == ERROR_SUCCESS; // ERROR_NOT_ALL_ASSIGNED if the privilege is not in the token
    CloseHandle(Token);
    return Result;
}

int main(int argc, char* argv[])
{
    if (!EnablePrivilege(L"SeProfileSingleProcessPrivilege") || !EnablePrivilege(L"SeIncreaseQuotaPrivilege"))
    {
        fprintf(stderr, "purge: privileges not available, run as administrator\n");
        return 1;
    }

    // Dirty data of the volume written to the disk
    wchar_t VolumeName[] = L"\\\\.\\C:";
    if (argc > 1 && argv[1][0])
        VolumeName[4] = (wchar_t)argv[1][0];
    auto Volume = CreateFileW(VolumeName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (Volume == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "purge: volume %c: can not be open (%lu)\n", (char)VolumeName[4], GetLastError());
        return 1;
    }
    if (!FlushFileBuffers(Volume))
    {
        auto Error = GetLastError();
        CloseHandle(Volume);
        fprintf(stderr, "purge: volume flush failed (%lu)\n", Error);
        return 1;
    }
    CloseHandle(Volume);

    // Working set of the system file cache moved to the standby list
    if (!SetSystemFileCacheSize((SIZE_T)-1, (SIZE_T)-1, 0))
    {
        fprintf(stderr, "purge: system file cache trim failed (%lu)\n", GetLastError());
        return 1;
    }

    // Modified pages written, then standby pages (the cache) freed
    auto NtSetSystemInformation_Ptr = (NtSetSystemInformation_Type)(void*)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtSetSystemInformation");
    if (!NtSetSystemInformation_Ptr)
    {
        fprintf(stderr, "purge: NtSetSystemInformation not found\n");
        return 1;
    }
    const INT SystemMemoryListInformation = 80;
    const int Commands[] = { 3, 4 }; // MemoryFlushModifiedList, MemoryPurgeStandbyList
    for (auto Command : Commands)
    {
        auto Status = NtSetSystemInformation_Ptr(SystemMemoryListInformation, (PVOID)&Command, sizeof(Command));
        if (Status < 0)
        {
            fprintf(stderr, "purge: memory list command %d failed (0x%08lX)\n", Command, (unsigned long)Status);
            return 1;
        }
    }
    return 0;
}

#else //defined(_WIN32) || defined(_WINDOWS)
#include <stdlib.h>
#include <unistd.h>

int main()
{
    // Dirty data written to the disk
    sync();

    #if defined(__APPLE__)
    // Purge command of the system
    if (system("/usr/sbin/purge"))
    {
        fprintf(stderr, "purge: /usr/sbin/purge failed, run as root (sudo)\n");
        return 1;
    }
    #else //defined(__APPLE__)
    // Page cache, dentries and inodes freed
    FILE* File = fopen("/proc/sys/vm/drop_caches", "w");
    if (!File)
    {
        fprintf(stderr, "purge: /proc/sys/vm/drop_caches can not be open, run as root (sudo)\n");
        return 1;
    }
    bool Result = fputs("3", File) < 0;
    if (fclose(File) || Result)
    {
        fprintf(stderr, "purge: /proc/sys/vm/drop_caches can not be written\n");
        return 1;
    }
    #endif //defined(__APPLE__)
    return 0;
}
#endif //defined(_WIN32) || defined(_WINDOWS)
