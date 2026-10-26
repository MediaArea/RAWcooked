/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFmpeg_Library.h"
#ifdef RAWCOOKED_FFMPEG
#include "Lib/CoDec/FFmpeg_Functions.h"
#include "Lib/Utils/Trace/Trace.h"
#include <atomic>
#include <cstring>
#include <vector>
#if (defined(_WIN32) || defined(__APPLE__)) && !defined(RAWCOOKED_FFMPEG_DYNAMIC)
    #define RAWCOOKED_FFMPEG_DYNAMIC
#endif
#ifdef RAWCOOKED_FFMPEG_DYNAMIC
    #if defined(_WIN32)
        #define WIN32_LEAN_AND_MEAN
        #ifndef NOMINMAX
            #define NOMINMAX
        #endif
        #include <windows.h>
    #else
        #include <climits>
        #include <cstdlib>
        #include <dlfcn.h>
        #include <unistd.h>
        #if defined(__APPLE__)
            #include <mach-o/dyld.h>
        #endif
    #endif
#endif
using namespace std;
//---------------------------------------------------------------------------

//***************************************************************************
// Functions
//***************************************************************************

//---------------------------------------------------------------------------
namespace ffmpeg
{
    #ifdef RAWCOOKED_FFMPEG_DYNAMIC
        #define FFMPEG_FUNCTION_DEFINE(_LIBRARY, _NAME) decltype(&::_NAME) _NAME = nullptr;
    #else
        #define FFMPEG_FUNCTION_DEFINE(_LIBRARY, _NAME) decltype(&::_NAME) _NAME = &::_NAME;
    #endif
    FFMPEG_FUNCTIONS(FFMPEG_FUNCTION_DEFINE)
    #undef FFMPEG_FUNCTION_DEFINE
}

//***************************************************************************
// Load
//***************************************************************************

#ifdef RAWCOOKED_FFMPEG_DYNAMIC
//---------------------------------------------------------------------------
// Library file names, with the major version of the headers (same ABI)
enum library_id
{
    Library_avutil,
    Library_avcodec,
    Library_Max
};
#if defined(_WIN32)
static const char* const Library_Names[Library_Max] = { "avutil-" AV_STRINGIFY(LIBAVUTIL_VERSION_MAJOR) ".dll", "avcodec-" AV_STRINGIFY(LIBAVCODEC_VERSION_MAJOR) ".dll" };
static const char* const Library_Locations = "the directory of the executable, in its ffmpeg\\bin subdirectory or in PATH";
typedef HMODULE library;
typedef wstring path;
#define FFMPEG_SYMBOL(_LIBRARY, _NAME) GetProcAddress(_LIBRARY, _NAME)
#else
#if defined(__APPLE__)
#define LIBRARY_NAME(_NAME, _SUFFIX, _MAJOR) "lib" _NAME _SUFFIX "." AV_STRINGIFY(_MAJOR) ".dylib"
static const char* const Library_Locations = "the directory of the executable, in its ffmpeg/lib or ffmpeg/bin subdirectory, in /opt/homebrew/lib, /usr/local/lib or /opt/local/lib";
#else
#define LIBRARY_NAME(_NAME, _SUFFIX, _MAJOR) "lib" _NAME _SUFFIX ".so." AV_STRINGIFY(_MAJOR)
static const char* const Library_Locations = "the directory of the executable, in its ffmpeg/lib or ffmpeg/bin subdirectory or in the library paths of the system";
#endif
static const char* const Library_Names[Library_Max] = { LIBRARY_NAME("avutil", "", LIBAVUTIL_VERSION_MAJOR), LIBRARY_NAME("avcodec", "", LIBAVCODEC_VERSION_MAJOR) };
static const char* const Library_Names_MA[Library_Max] = { LIBRARY_NAME("avutil", "-ma", LIBAVUTIL_VERSION_MAJOR), LIBRARY_NAME("avcodec", "-ma", LIBAVCODEC_VERSION_MAJOR) }; // ffmpeg-ma package (FFmpeg build of MediaArea, with -ma suffix)
#undef LIBRARY_NAME
typedef void* library;
typedef string path;
#define FFMPEG_SYMBOL(_LIBRARY, _NAME) dlsym(_LIBRARY, _NAME)
#endif

//---------------------------------------------------------------------------
#if defined(_WIN32)
static string Path_ToString(const wstring& Path)
{
    auto Size = WideCharToMultiByte(CP_UTF8, 0, Path.c_str(), (int)Path.size(), nullptr, 0, nullptr, nullptr);
    string Result(Size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, Path.c_str(), (int)Path.size(), &Result[0], Size, nullptr, nullptr);
    return Result;
}
#else
static const string& Path_ToString(const string& Path)
{
    return Path;
}
#endif

//---------------------------------------------------------------------------
#if defined(_WIN32)
// Directories where the libraries are searched, the current directory is not used
static vector<path> Library_Directories()
{
    vector<path> Directories;

    // Directory of the executable
    wstring Path(MAX_PATH, L'\0');
    for (;;)
    {
        auto Size = GetModuleFileNameW(nullptr, &Path[0], (DWORD)Path.size());
        if (!Size)
            break;
        if (Size < Path.size())
        {
            Path.resize(Size);
            auto Separator = Path.find_last_of(L"\\/");
            if (Separator != wstring::npos)
            {
                Directories.push_back(Path.substr(0, Separator));
                Directories.push_back(Path.substr(0, Separator) + L"\\ffmpeg\\bin"); // FFmpeg package next to the executable
            }
            break;
        }
        Path.resize(Path.size() * 2);
    }

    // Absolute directories in PATH
    auto Env_Size = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    wstring Env(Env_Size, L'\0');
    if (Env_Size && (Env_Size = GetEnvironmentVariableW(L"PATH", &Env[0], Env_Size)) < Env.size())
    {
        Env.resize(Env_Size);
        for (size_t Begin = 0; Begin < Env.size();)
        {
            auto End = Env.find(L';', Begin);
            if (End == wstring::npos)
                End = Env.size();
            auto Directory = Env.substr(Begin, End - Begin);
            Begin = End + 1;
            if (Directory.size() >= 2 && Directory.front() == L'"' && Directory.back() == L'"')
                Directory = Directory.substr(1, Directory.size() - 2);
            if (!((Directory.size() >= 3 && Directory[1] == L':' && (Directory[2] == L'\\' || Directory[2] == L'/')) || (Directory.size() >= 2 && Directory[0] == L'\\' && Directory[1] == L'\\')))
                continue;
            while (Directory.back() == L'\\' || Directory.back() == L'/')
                Directory.pop_back();
            Directories.push_back(Directory);
        }
    }

    return Directories;
}
#else
// Paths of libavcodec, in priority order, the current directory is not used:
// directory of the executable and its ffmpeg/lib and ffmpeg/bin subdirectories (FFmpeg package next to the executable),
// then libraries of the ffmpeg-ma package, then other FFmpeg libraries
// A file name without directory means the default search of the system
static vector<path> Library_Paths()
{
    vector<path> Paths;

    // Directory of the executable
    string Path;
    #if defined(__APPLE__)
    uint32_t Size = 0;
    _NSGetExecutablePath(nullptr, &Size);
    Path.resize(Size);
    if (_NSGetExecutablePath(&Path[0], &Size))
        Path.clear();
    Path.resize(strlen(Path.c_str()));
    #else
    char Link[PATH_MAX];
    auto Size = readlink("/proc/self/exe", Link, sizeof(Link) - 1);
    if (Size > 0)
        Path.assign(Link, (size_t)Size);
    #endif
    char Real[PATH_MAX];
    if (!Path.empty() && realpath(Path.c_str(), Real))
        Path = Real; // Real directory of the executable if it is called through a link
    auto Separator = Path.rfind('/');
    if (Separator != string::npos)
    {
        Path.resize(Separator); // Empty for the root directory
        for (const auto& Subdirectory : { "", "/ffmpeg/lib", "/ffmpeg/bin" })
            for (const auto& Names : { Library_Names_MA, Library_Names })
                Paths.push_back(Path + Subdirectory + '/' + Names[Library_avcodec]);
    }

    // ffmpeg-ma package, then other FFmpeg libraries
    for (const auto& Names : { Library_Names_MA, Library_Names })
    {
        #if defined(__APPLE__)
        // Usual locations (Homebrew Apple silicon, ffmpeg-ma package and Homebrew Intel, MacPorts)
        for (const auto& Directory : { "/opt/homebrew/lib", "/usr/local/lib", "/opt/local/lib" })
            Paths.push_back(string(Directory) + '/' + Names[Library_avcodec]);
        #else
        Paths.push_back(Names[Library_avcodec]);
        #endif
    }

    return Paths;
}
#endif

//---------------------------------------------------------------------------
static void Library_Functions_Clear()
{
    #define FFMPEG_FUNCTION_CLEAR(_LIBRARY, _NAME) ffmpeg::_NAME = nullptr;
    FFMPEG_FUNCTIONS(FFMPEG_FUNCTION_CLEAR)
    #undef FFMPEG_FUNCTION_CLEAR
}

//---------------------------------------------------------------------------
static bool Library_Functions(library Libraries[Library_Max], const path& Path, string& Error)
{
    const char* Missing = nullptr;
    #define FFMPEG_FUNCTION_LOAD(_LIBRARY, _NAME) \
        if (!(ffmpeg::_NAME = reinterpret_cast<decltype(ffmpeg::_NAME)>(FFMPEG_SYMBOL(Libraries[Library_##_LIBRARY], #_NAME)))) \
            Missing = #_NAME;
    FFMPEG_FUNCTIONS(FFMPEG_FUNCTION_LOAD)
    #undef FFMPEG_FUNCTION_LOAD
    if (Missing)
        Error = "FFmpeg library " + Path_ToString(Path) + " has no function " + Missing;
    else if ((ffmpeg::avutil_version() >> 16) != LIBAVUTIL_VERSION_MAJOR || (ffmpeg::avcodec_version() >> 16) != LIBAVCODEC_VERSION_MAJOR)
        Error = "FFmpeg library " + Path_ToString(Path) + " has an incompatible version";
    else
        return true;
    Library_Functions_Clear();
    return false;
}

//---------------------------------------------------------------------------
static bool Library_Load(string& Error)
{
    Error.clear();

    #if defined(_WIN32)
    wstring Names[Library_Max];
    for (size_t i = 0; i < Library_Max; i++)
        Names[i].assign(Library_Names[i], Library_Names[i] + strlen(Library_Names[i]));
    DWORD ErrorMode;
    SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &ErrorMode); // No message box from the system if a library is invalid
    auto Loaded = false;
    for (const auto& Directory : Library_Directories())
    {
        auto Path = Directory + L'\\' + Names[Library_avcodec];
        if (GetFileAttributesW(Path.c_str()) == INVALID_FILE_ATTRIBUTES)
            continue;
        library Libraries[Library_Max];
        Libraries[Library_avcodec] = LoadLibraryExW(Path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS); // Dependencies are searched in the directory of the library, of the executable and of the system, not in the current directory
        if (!Libraries[Library_avcodec])
        {
            string Reason;
            switch (auto Code = GetLastError())
            {
                case ERROR_MOD_NOT_FOUND: Reason = "a library it depends on is not found in the same directory"; break;
                case ERROR_PROC_NOT_FOUND: Reason = "a library it depends on is incompatible"; break;
                case ERROR_BAD_EXE_FORMAT: Reason = "not a library for this platform"; break;
                default: Reason = "error " + to_string(Code);
            }
            Error = "FFmpeg library " + Path_ToString(Path) + " can not be loaded, " + Reason;
            continue;
        }
        Libraries[Library_avutil] = GetModuleHandleW(Names[Library_avutil].c_str()); // Loaded as a dependency of libavcodec
        if (!Libraries[Library_avutil])
            Error = "FFmpeg library " + Path_ToString(Path) + " does not use " + Library_Names[Library_avutil];
        else if (Library_Functions(Libraries, Path, Error))
        {
            Loaded = true;
            break;
        }
        FreeLibrary(Libraries[Library_avcodec]);
    }
    SetThreadErrorMode(ErrorMode, nullptr);
    if (Loaded)
        return true;
    #else
    for (const auto& Path : Library_Paths())
    {
        auto HasDirectory = Path.find('/') != string::npos;
        if (HasDirectory && access(Path.c_str(), F_OK))
            continue;
        library Libraries[Library_Max];
        Libraries[Library_avcodec] = dlopen(Path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!Libraries[Library_avcodec])
        {
            auto Reason = dlerror();
            if (HasDirectory)
                Error = "FFmpeg library " + Path + " can not be loaded, " + (Reason ? Reason : "unknown error");
            continue;
        }
        Libraries[Library_avutil] = Libraries[Library_avcodec]; // Symbols of libavutil are found in the dependencies of libavcodec
        if (Library_Functions(Libraries, Path, Error))
            return true;
        dlclose(Libraries[Library_avcodec]);
    }
    #endif

    if (Error.empty())
    {
        Error = string("FFmpeg libraries ") + Library_Names[Library_avcodec] + " and " + Library_Names[Library_avutil];
        #if !defined(_WIN32)
        Error += string(" (or ") + Library_Names_MA[Library_avcodec] + " and " + Library_Names_MA[Library_avutil] + " of ffmpeg-ma)";
        #endif
        Error += string(" are not found in ") + Library_Locations;
    }
    return false;
}
#else //RAWCOOKED_FFMPEG_DYNAMIC
//---------------------------------------------------------------------------
static bool Library_Load(string&)
{
    return true; // Linked
}
#endif //RAWCOOKED_FFMPEG_DYNAMIC

//---------------------------------------------------------------------------
bool FFmpeg_Library_Load(string* Error)
{
    static string Load_Error;
    static const bool Loaded = [] { TRACE_SCOPE("FFmpeg load"); return Library_Load(Load_Error); }(); // Once, thread safe
    if (!Loaded && Error)
        *Error = Load_Error;
    return Loaded;
}

//***************************************************************************
// Vulkan
//***************************************************************************

//---------------------------------------------------------------------------
// Begin of AVVulkanDeviceContext (libavutil/hwcontext_vulkan.h) and of VkPhysicalDeviceProperties (Vulkan 1.0 ABI, frozen), so Vulkan headers are not needed
#if defined(_WIN32)
    #define VULKAN_CALL __stdcall
#else
    #define VULKAN_CALL
#endif
typedef void (VULKAN_CALL* vulkan_void_function)();
typedef vulkan_void_function (VULKAN_CALL* vulkan_get_instance_proc_addr)(void* Instance, const char* Name);
typedef void (VULKAN_CALL* vulkan_get_physical_device_properties)(void* PhysicalDevice, void* Properties);
struct vulkan_device_context
{
    const void*                     alloc;
    vulkan_get_instance_proc_addr   get_proc_addr;
    void*                           inst;
    void*                           phys_dev;
};
struct vulkan_physical_device_properties
{
    uint32_t                        apiVersion;
    uint32_t                        driverVersion;
    uint32_t                        vendorID;
    uint32_t                        deviceID;
    uint32_t                        deviceType;
    char                            deviceName[256];
    uint64_t                        Others[128]; // pipelineCacheUUID, limits, sparseProperties (548 bytes)
};
static const uint32_t Vulkan_PhysicalDeviceType_Cpu = 4; // VK_PHYSICAL_DEVICE_TYPE_CPU

//---------------------------------------------------------------------------
// Vulkan device is a GPU, not a CPU implementation (e.g. lavapipe)
static bool Vulkan_IsGPU(AVBufferRef* Device)
{
    auto Vulkan = (const vulkan_device_context*)((AVHWDeviceContext*)Device->data)->hwctx;
    auto GetPhysicalDeviceProperties = Vulkan->get_proc_addr ? (vulkan_get_physical_device_properties)Vulkan->get_proc_addr(Vulkan->inst, "vkGetPhysicalDeviceProperties") : nullptr;
    if (!GetPhysicalDeviceProperties)
        return false;
    vulkan_physical_device_properties Properties;
    GetPhysicalDeviceProperties(Vulkan->phys_dev, &Properties);
    return Properties.deviceType != Vulkan_PhysicalDeviceType_Cpu;
}

//---------------------------------------------------------------------------
static atomic<AVBufferRef*> Vulkan_Device(nullptr); // Device created by FFmpeg_Library_Vulkan(), until a decoder uses it

//---------------------------------------------------------------------------
// Default Vulkan device of FFmpeg, with imports of host memory for the downloads of decoded frames (no copy from an intermediate buffer):
// FFmpeg avoids them by default with the NVIDIA driver as they are slow in some environments, but with frames reused by the decoder they are faster
// (measured with NVIDIA RTX 3050 on Windows, 6464x4852 RGB 16-bit: GPU time per frame 266 ms instead of 284 ms, no CPU copy)
static AVBufferRef* Vulkan_Device_Create()
{
    TRACE_SCOPE("Vulkan device creation");
    AVBufferRef* Device = nullptr;
    AVDictionary* Options = nullptr;
    ffmpeg::av_dict_set(&Options, "avoid_host_import", "0", 0);
    if (ffmpeg::av_hwdevice_ctx_create(&Device, AV_HWDEVICE_TYPE_VULKAN, nullptr, Options, 0) < 0)
        Device = nullptr;
    ffmpeg::av_dict_free(&Options);
    return Device;
}

//---------------------------------------------------------------------------
enum class vulkan_device
{
    none,
    cpu,
    gpu,
};

//---------------------------------------------------------------------------
bool FFmpeg_Library_Vulkan(bool AllowCPU)
{
    static const vulkan_device Device_Kind = []() -> vulkan_device // Once, thread safe
    {
        if (!FFmpeg_Library_Load())
            return vulkan_device::none;
        ffmpeg::av_log_set_level(AV_LOG_QUIET); // No message if Vulkan is not available, or if the decoding fails later (the caller falls back to our decoder)

        // Same device as the one created by the decoder, kept for the decoder (device creation is long)
        auto Device = Vulkan_Device_Create();
        if (!Device)
            return vulkan_device::none;
        Vulkan_Device = Device;
        return Vulkan_IsGPU(Device) ? vulkan_device::gpu : vulkan_device::cpu;
    }();
    if (Device_Kind == vulkan_device::gpu || (Device_Kind == vulkan_device::cpu && AllowCPU))
        return true;
    if (auto Device = Vulkan_Device.exchange(nullptr))
        ffmpeg::av_buffer_unref(&Device); // Not used
    return false;
}

//---------------------------------------------------------------------------
AVBufferRef* FFmpeg_Library_VulkanDevice()
{
    if (auto Device = Vulkan_Device.exchange(nullptr))
        return Device;
    return Vulkan_Device_Create();
}

#else //RAWCOOKED_FFMPEG

//---------------------------------------------------------------------------
bool FFmpeg_Library_Load(std::string* Error)
{
    if (Error)
        *Error = "FFmpeg is not supported in this build";
    return false;
}

//---------------------------------------------------------------------------
bool FFmpeg_Library_Vulkan(bool)
{
    return false;
}

#endif //RAWCOOKED_FFMPEG
