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
#if (defined(_WIN32) || defined(__APPLE__)) && !defined(RAWCOOKED_FFMPEG_DYNAMIC)
    #define RAWCOOKED_FFMPEG_DYNAMIC
#endif
#endif //RAWCOOKED_FFMPEG
#include <atomic>
#include <cstring>
#include <vector>
#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <climits>
    #include <cstdlib>
    #include <sys/stat.h>
    #include <unistd.h>
    #if defined(__APPLE__)
        #include <mach-o/dyld.h>
    #endif
    #ifdef RAWCOOKED_FFMPEG_DYNAMIC
        #include <dlfcn.h>
    #endif
#endif
using namespace std;
//---------------------------------------------------------------------------

//***************************************************************************
// FFmpeg package next to the executable
//***************************************************************************

//---------------------------------------------------------------------------
#if defined(_WIN32)
typedef wstring path;
static const wchar_t* const Package_Subdirectories[] = { L"", L"\\ffmpeg", L"\\ffmpeg\\lib", L"\\ffmpeg\\bin" };
#else
typedef string path;
static const char* const Package_Subdirectories[] = { "", "/ffmpeg", "/ffmpeg/lib", "/ffmpeg/bin" };
#endif

//---------------------------------------------------------------------------
// Directories where the binary and the libraries of FFmpeg are searched first, in priority order:
// directory of the executable and its ffmpeg, ffmpeg/lib and ffmpeg/bin subdirectories (FFmpeg package next to the executable)
// Real directory of the executable if it is called through a link, except on Windows
static vector<path> Package_Directories()
{
    vector<path> Directories;

    // Path of the executable
    #if defined(_WIN32)
    wstring Path(MAX_PATH, L'\0');
    for (;;)
    {
        auto Size = GetModuleFileNameW(nullptr, &Path[0], (DWORD)Path.size());
        if (!Size)
            return Directories;
        if (Size < Path.size())
        {
            Path.resize(Size);
            break;
        }
        Path.resize(Path.size() * 2);
    }
    auto Separator = Path.find_last_of(L"\\/");
    #else
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
        Path = Real;
    auto Separator = Path.rfind('/');
    #endif
    if (Separator == path::npos)
        return Directories;

    Path.resize(Separator); // Empty for the root directory on Unix-like systems
    for (const auto& Subdirectory : Package_Subdirectories)
        Directories.push_back(Path + Subdirectory);
    return Directories;
}

//---------------------------------------------------------------------------
// Absolute directories in PATH, in priority order
static vector<path> Path_Directories()
{
    vector<path> Directories;

    #if defined(_WIN32)
    auto Env_Size = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    wstring Env(Env_Size, L'\0');
    if (!Env_Size || (Env_Size = GetEnvironmentVariableW(L"PATH", &Env[0], Env_Size)) >= Env.size())
        return Directories;
    Env.resize(Env_Size);
    const auto Separator = L';';
    #else
    auto Env_Value = getenv("PATH");
    if (!Env_Value)
        return Directories;
    string Env(Env_Value);
    const auto Separator = ':';
    #endif
    for (size_t Begin = 0; Begin < Env.size();)
    {
        auto End = Env.find(Separator, Begin);
        if (End == path::npos)
            End = Env.size();
        auto Directory = Env.substr(Begin, End - Begin);
        Begin = End + 1;
        #if defined(_WIN32)
        if (Directory.size() >= 2 && Directory.front() == L'"' && Directory.back() == L'"')
            Directory = Directory.substr(1, Directory.size() - 2);
        if (!((Directory.size() >= 3 && Directory[1] == L':' && (Directory[2] == L'\\' || Directory[2] == L'/')) || (Directory.size() >= 2 && Directory[0] == L'\\' && Directory[1] == L'\\')))
            continue;
        while (Directory.back() == L'\\' || Directory.back() == L'/')
            Directory.pop_back();
        #else
        if (Directory.empty() || Directory.front() != '/')
            continue;
        while (Directory.size() > 1 && Directory.back() == '/')
            Directory.pop_back();
        if (Directory == "/")
            Directory.clear(); // Root directory
        #endif
        Directories.push_back(Directory);
    }

    return Directories;
}

//---------------------------------------------------------------------------
// Path of the binary if it is an executable file, as a string usable in a command line
static bool Binary_Path(const path& Directory, const char* Name, string& Result)
{
    #if defined(_WIN32)
    auto Path = Directory + L'\\' + wstring(Name, Name + strlen(Name)) + L".exe";
    auto Attributes = GetFileAttributesW(Path.c_str());
    if (Attributes == INVALID_FILE_ATTRIBUTES || (Attributes & FILE_ATTRIBUTE_DIRECTORY))
        return false;

    // Command lines are in the ANSI code page, the path is ignored if it can not be represented in it
    BOOL UsedDefaultChar = FALSE;
    auto Size = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, Path.c_str(), (int)Path.size(), nullptr, 0, nullptr, &UsedDefaultChar);
    if (!Size || UsedDefaultChar)
        return false;
    Result.assign(Size, '\0');
    WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, Path.c_str(), (int)Path.size(), &Result[0], Size, nullptr, nullptr);
    #else
    auto Path = Directory + '/' + Name;
    struct stat Stat;
    if (stat(Path.c_str(), &Stat) || !S_ISREG(Stat.st_mode) || access(Path.c_str(), X_OK))
        return false;
    Result = Path;
    #endif
    return true;
}

//---------------------------------------------------------------------------
string FFmpeg_Binary()
{
    string Result;

    // ffmpeg-ma (FFmpeg build of MediaArea, with -ma suffix) next to the executable then in PATH,
    // then ffmpeg next to the executable (ffmpeg in PATH is found by the system)
    auto Directories = Package_Directories();
    auto Package_Count = Directories.size();
    for (auto& Directory : Path_Directories())
        Directories.push_back(move(Directory));
    for (const auto& Directory : Directories)
        if (Binary_Path(Directory, "ffmpeg-ma", Result))
            return Result;
    for (size_t i = 0; i < Package_Count; i++)
        if (Binary_Path(Directories[i], "ffmpeg", Result))
            return Result;

    return string();
}

//---------------------------------------------------------------------------
static string Library_Path_Provided; // libavcodec provided by the user, used instead of the search

//---------------------------------------------------------------------------
bool FFmpeg_Library_SetPath(const string& FileName)
{
    // File name of libavcodec, any version (an incompatible version is rejected when loaded)
    #if defined(_WIN32)
    auto Separator = FileName.find_last_of("\\/");
    #else
    auto Separator = FileName.rfind('/');
    #endif
    auto Name = FileName.substr(Separator == string::npos ? 0 : (Separator + 1));
    #if defined(_WIN32)
    for (auto& Char : Name)
        if (Char >= 'A' && Char <= 'Z')
            Char += 'a' - 'A';
    auto IsLibrary = !Name.compare(0, 7, "avcodec") && Name.size() > 11 && !Name.compare(Name.size() - 4, 4, ".dll");
    #elif defined(__APPLE__)
    auto IsLibrary = !Name.compare(0, 10, "libavcodec") && Name.size() > 16 && !Name.compare(Name.size() - 6, 6, ".dylib");
    #else
    auto IsLibrary = !Name.compare(0, 10, "libavcodec") && Name.find(".so", 10) != string::npos;
    #endif
    if (!IsLibrary)
        return false;
    Library_Path_Provided = FileName;
    return true;
}

#ifdef RAWCOOKED_FFMPEG
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
static const char* const Library_Locations = "the directory of the executable, in its ffmpeg, ffmpeg\\lib or ffmpeg\\bin subdirectory or in PATH";
typedef HMODULE library;
#define FFMPEG_SYMBOL(_LIBRARY, _NAME) GetProcAddress(_LIBRARY, _NAME)
#else
#if defined(__APPLE__)
#define LIBRARY_NAME(_NAME, _SUFFIX, _MAJOR) "lib" _NAME _SUFFIX "." AV_STRINGIFY(_MAJOR) ".dylib"
static const char* const Library_Locations = "the directory of the executable, in its ffmpeg, ffmpeg/lib or ffmpeg/bin subdirectory, in /opt/homebrew/lib, /usr/local/lib or /opt/local/lib";
#else
#define LIBRARY_NAME(_NAME, _SUFFIX, _MAJOR) "lib" _NAME _SUFFIX ".so." AV_STRINGIFY(_MAJOR)
static const char* const Library_Locations = "the directory of the executable, in its ffmpeg, ffmpeg/lib or ffmpeg/bin subdirectory or in the library paths of the system";
#endif
static const char* const Library_Names[Library_Max] = { LIBRARY_NAME("avutil", "", LIBAVUTIL_VERSION_MAJOR), LIBRARY_NAME("avcodec", "", LIBAVCODEC_VERSION_MAJOR) };
static const char* const Library_Names_MA[Library_Max] = { LIBRARY_NAME("avutil", "-ma", LIBAVUTIL_VERSION_MAJOR), LIBRARY_NAME("avcodec", "-ma", LIBAVCODEC_VERSION_MAJOR) }; // ffmpeg-ma package (FFmpeg build of MediaArea, with -ma suffix)
#undef LIBRARY_NAME
typedef void* library;
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
    // FFmpeg package next to the executable, then absolute directories in PATH
    auto Directories = Package_Directories();
    for (auto& Directory : Path_Directories())
        Directories.push_back(move(Directory));
    return Directories;
}
#else
// Paths of libavcodec, in priority order, the current directory is not used:
// libraries of the ffmpeg-ma package, then other FFmpeg libraries, each in the directory of the executable
// and its ffmpeg, ffmpeg/lib and ffmpeg/bin subdirectories (FFmpeg package next to the executable) then in the system
// A file name without directory means the default search of the system
static vector<path> Library_Paths()
{
    vector<path> Paths;

    // ffmpeg-ma package, then other FFmpeg libraries, next to the executable then in the system
    auto Directories = Package_Directories();
    for (const auto& Names : { Library_Names_MA, Library_Names })
    {
        for (const auto& Directory : Directories)
            Paths.push_back(Directory + '/' + Names[Library_avcodec]);
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
    vector<path> Paths;
    if (!Library_Path_Provided.empty())
    {
        auto Size = MultiByteToWideChar(CP_ACP, 0, Library_Path_Provided.c_str(), (int)Library_Path_Provided.size(), nullptr, 0); // Command line is in the ANSI code page
        wstring Path(Size, L'\0');
        MultiByteToWideChar(CP_ACP, 0, Library_Path_Provided.c_str(), (int)Library_Path_Provided.size(), &Path[0], Size);
        Paths.push_back(Path);
    }
    else
        for (const auto& Directory : Library_Directories())
            Paths.push_back(Directory + L'\\' + Names[Library_avcodec]);
    DWORD ErrorMode;
    SetThreadErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &ErrorMode); // No message box from the system if a library is invalid
    auto Loaded = false;
    for (const auto& Path : Paths)
    {
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
    auto Paths = Library_Path_Provided.empty() ? Library_Paths() : vector<path>{ Library_Path_Provided };
    for (const auto& Path : Paths)
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

    if (Error.empty() && !Library_Path_Provided.empty())
        Error = "FFmpeg library " + Library_Path_Provided + " is not found";
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
static bool Library_Load(string& Error)
{
    if (!Library_Path_Provided.empty())
    {
        Error = "FFmpeg libraries are linked in this build, FFmpeg library " + Library_Path_Provided + " can not be used";
        return false;
    }
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
typedef int32_t (VULKAN_CALL* vulkan_enumerate_device_extension_properties)(void* PhysicalDevice, const char* LayerName, uint32_t* Count, void* Properties);
typedef int32_t (VULKAN_CALL* vulkan_enumerate_physical_devices)(void* Instance, uint32_t* Count, void** PhysicalDevices);
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
static const uint32_t Vulkan_PhysicalDeviceType_DiscreteGpu = 2; // VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
static const uint32_t Vulkan_PhysicalDeviceType_Cpu = 4; // VK_PHYSICAL_DEVICE_TYPE_CPU
struct vulkan_physical_device_properties2 // Vulkan 1.1
{
    uint32_t                        sType;
    void*                           pNext;
    vulkan_physical_device_properties properties;
};
static const uint32_t Vulkan_StructureType_PhysicalDeviceProperties2 = 1000059001; // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
struct vulkan_physical_device_shader_sm_builtins_properties // VK_NV_shader_sm_builtins
{
    uint32_t                        sType;
    void*                           pNext;
    uint32_t                        shaderSMCount;
    uint32_t                        shaderWarpsPerSM;
};
static const uint32_t Vulkan_StructureType_PhysicalDeviceShaderSMBuiltinsProperties = 1000154001; // VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_SM_BUILTINS_PROPERTIES_NV
struct vulkan_extension_properties
{
    char                            extensionName[256];
    uint32_t                        specVersion;
};

//---------------------------------------------------------------------------
// Count of streaming multiprocessors of an NVIDIA GPU, 0 if unknown
static uint32_t Vulkan_SMCount(const vulkan_device_context* Vulkan)
{
    auto EnumerateDeviceExtensionProperties = (vulkan_enumerate_device_extension_properties)Vulkan->get_proc_addr(Vulkan->inst, "vkEnumerateDeviceExtensionProperties");
    auto GetPhysicalDeviceProperties2 = (vulkan_get_physical_device_properties)Vulkan->get_proc_addr(Vulkan->inst, "vkGetPhysicalDeviceProperties2");
    uint32_t Count = 0;
    if (!EnumerateDeviceExtensionProperties || !GetPhysicalDeviceProperties2 || EnumerateDeviceExtensionProperties(Vulkan->phys_dev, nullptr, &Count, nullptr) < 0)
        return 0;
    vector<vulkan_extension_properties> Extensions(Count);
    if (!Count || EnumerateDeviceExtensionProperties(Vulkan->phys_dev, nullptr, &Count, Extensions.data()) < 0)
        return 0;
    for (uint32_t i = 0; i < Count; i++)
        if (!strcmp(Extensions[i].extensionName, "VK_NV_shader_sm_builtins"))
        {
            vulkan_physical_device_shader_sm_builtins_properties SM = { Vulkan_StructureType_PhysicalDeviceShaderSMBuiltinsProperties, nullptr, 0, 0 };
            vulkan_physical_device_properties2 Properties = { Vulkan_StructureType_PhysicalDeviceProperties2, &SM, {} };
            GetPhysicalDeviceProperties2(Vulkan->phys_dev, &Properties);
            return SM.shaderSMCount;
        }
    return 0;
}

//---------------------------------------------------------------------------
// Vulkan device is a GPU, not a CPU implementation (e.g. lavapipe), and its name
static bool Vulkan_IsGPU(AVBufferRef* Device, string& Name)
{
    auto Vulkan = (const vulkan_device_context*)((AVHWDeviceContext*)Device->data)->hwctx;
    auto GetPhysicalDeviceProperties = Vulkan->get_proc_addr ? (vulkan_get_physical_device_properties)Vulkan->get_proc_addr(Vulkan->inst, "vkGetPhysicalDeviceProperties") : nullptr;
    if (!GetPhysicalDeviceProperties)
        return false;
    vulkan_physical_device_properties Properties;
    GetPhysicalDeviceProperties(Vulkan->phys_dev, &Properties);
    Name.assign(Properties.deviceName, strnlen(Properties.deviceName, sizeof(Properties.deviceName)));
    return Properties.deviceType != Vulkan_PhysicalDeviceType_Cpu;
}

//---------------------------------------------------------------------------
static atomic<AVBufferRef*> Vulkan_Device(nullptr); // Default device created by Vulkan_Detect(), until a decoder uses it

//---------------------------------------------------------------------------
// Vulkan device of FFmpeg (Index: index of the device in the list of devices, 0 for the default device), with imports of host memory for the downloads of decoded frames (no copy from an intermediate buffer):
// FFmpeg avoids them by default with the NVIDIA driver as they are slow in some environments, but with frames reused by the decoder they are faster
// (measured with NVIDIA RTX 3050 on Windows, 6464x4852 RGB 16-bit: GPU time per frame 266 ms instead of 284 ms, no CPU copy)
static AVBufferRef* Vulkan_Device_Create(size_t Index)
{
    TRACE_SCOPE("Vulkan device creation");
    AVBufferRef* Device = nullptr;
    AVDictionary* Options = nullptr;
    ffmpeg::av_dict_set(&Options, "avoid_host_import", "0", 0);
    auto Index_String = to_string(Index);
    if (ffmpeg::av_hwdevice_ctx_create(&Device, AV_HWDEVICE_TYPE_VULKAN, Index ? Index_String.c_str() : nullptr, Options, 0) < 0)
        Device = nullptr;
    ffmpeg::av_dict_free(&Options);
    return Device;
}

//---------------------------------------------------------------------------
struct vulkan_device_info
{
    string                          Name;
    uint32_t                        Type; // VkPhysicalDeviceType
};

//---------------------------------------------------------------------------
static vulkan_device_info Vulkan_Info(vulkan_get_physical_device_properties GetPhysicalDeviceProperties, void* PhysicalDevice)
{
    vulkan_physical_device_properties Properties;
    GetPhysicalDeviceProperties(PhysicalDevice, &Properties);
    return { string(Properties.deviceName, strnlen(Properties.deviceName, sizeof(Properties.deviceName))), Properties.deviceType };
}

//---------------------------------------------------------------------------
// Vulkan devices, in the order of their index for FFmpeg, listed with the instance of the default device (empty if Vulkan is not available), this device is kept for the decoder (device creation is long)
static const vector<vulkan_device_info>& Vulkan_Detect()
{
    static const vector<vulkan_device_info> Devices = []() -> vector<vulkan_device_info> // Once, thread safe
    {
        vector<vulkan_device_info> Result;
        if (!FFmpeg_Library_Load())
            return Result;

        // Default device, same as the one created by the decoder
        auto Device = Vulkan_Device_Create(0);
        if (!Device)
            return Result;
        Vulkan_Device = Device;
        auto Vulkan = (const vulkan_device_context*)((AVHWDeviceContext*)Device->data)->hwctx;
        auto GetPhysicalDeviceProperties = Vulkan->get_proc_addr ? (vulkan_get_physical_device_properties)Vulkan->get_proc_addr(Vulkan->inst, "vkGetPhysicalDeviceProperties") : nullptr;
        if (!GetPhysicalDeviceProperties)
            return Result;

        // All devices
        auto EnumeratePhysicalDevices = (vulkan_enumerate_physical_devices)Vulkan->get_proc_addr(Vulkan->inst, "vkEnumeratePhysicalDevices");
        uint32_t Count = 0;
        if (EnumeratePhysicalDevices && EnumeratePhysicalDevices(Vulkan->inst, &Count, nullptr) >= 0 && Count)
        {
            vector<void*> PhysicalDevices(Count);
            if (EnumeratePhysicalDevices(Vulkan->inst, &Count, PhysicalDevices.data()) >= 0)
                for (uint32_t i = 0; i < Count; i++)
                    Result.push_back(Vulkan_Info(GetPhysicalDeviceProperties, PhysicalDevices[i]));
        }
        if (Result.empty())
            Result.push_back(Vulkan_Info(GetPhysicalDeviceProperties, Vulkan->phys_dev)); // Default device only
        return Result;
    }();
    return Devices;
}

//---------------------------------------------------------------------------
vector<size_t> FFmpeg_Library_Vulkan(bool AllowCPU)
{
    static const bool Quiet = [] // Once, before the detection
    {
        if (FFmpeg_Library_Load())
            ffmpeg::av_log_set_level(AV_LOG_QUIET); // No message if Vulkan is not available, or if the decoding fails later (the caller falls back to our decoder)
        return true;
    }();
    (void)Quiet;

    // All discrete GPUs, else the first GPU (e.g. an integrated GPU, sharing the memory and the power with the CPU), else the default device if a CPU implementation is allowed
    const auto& Devices = Vulkan_Detect();
    vector<size_t> Result;
    for (size_t i = 0; i < Devices.size(); i++)
        if (Devices[i].Type == Vulkan_PhysicalDeviceType_DiscreteGpu)
            Result.push_back(i);
    for (size_t i = 0; Result.empty() && i < Devices.size(); i++)
        if (Devices[i].Type != Vulkan_PhysicalDeviceType_Cpu)
            Result.push_back(i);
    if (Result.empty() && !Devices.empty() && AllowCPU)
        Result.push_back(0);
    if (Result.empty() || Result[0])
        if (auto Device = Vulkan_Device.exchange(nullptr))
            ffmpeg::av_buffer_unref(&Device); // Not used
    return Result;
}

//---------------------------------------------------------------------------
string FFmpeg_Library_VulkanName(bool* IsGPU)
{
    const auto& Devices = Vulkan_Detect();
    if (IsGPU)
        *IsGPU = !Devices.empty() && Devices[0].Type != Vulkan_PhysicalDeviceType_Cpu;
    return Devices.empty() ? string() : Devices[0].Name;
}

//---------------------------------------------------------------------------
AVBufferRef* FFmpeg_Library_VulkanDevice(size_t Index)
{
    if (!Index)
        if (auto Device = Vulkan_Device.exchange(nullptr))
            return Device;
    return Vulkan_Device_Create(Index);
}

//---------------------------------------------------------------------------
bool FFmpeg_Library_VulkanIsGPU(AVBufferRef* Device, uint32_t* SMCount, string* Name)
{
    string Name_;
    auto IsGPU = Vulkan_IsGPU(Device, Name_);
    if (SMCount)
        *SMCount = IsGPU ? Vulkan_SMCount((const vulkan_device_context*)((AVHWDeviceContext*)Device->data)->hwctx) : 0;
    if (Name)
        *Name = Name_;
    return IsGPU;
}

#endif //RAWCOOKED_FFMPEG

//---------------------------------------------------------------------------
static atomic<size_t> Vulkan_Missing((size_t)-1);

//---------------------------------------------------------------------------
void FFmpeg_Library_SetVulkanMissing(size_t Device)
{
    Vulkan_Missing = Device;
}

//---------------------------------------------------------------------------
size_t FFmpeg_Library_VulkanMissing()
{
    return Vulkan_Missing;
}

#ifndef RAWCOOKED_FFMPEG

//---------------------------------------------------------------------------
bool FFmpeg_Library_Load(std::string* Error)
{
    if (Error)
        *Error = "FFmpeg is not supported in this build";
    return false;
}

//---------------------------------------------------------------------------
vector<size_t> FFmpeg_Library_Vulkan(bool)
{
    return vector<size_t>();
}

//---------------------------------------------------------------------------
string FFmpeg_Library_VulkanName(bool* IsGPU)
{
    if (IsGPU)
        *IsGPU = false;
    return string();
}

#endif //RAWCOOKED_FFMPEG
