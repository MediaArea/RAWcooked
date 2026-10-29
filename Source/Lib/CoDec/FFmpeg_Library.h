/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFmpeg_LibraryH
#define FFmpeg_LibraryH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <string>
#include <vector>
//---------------------------------------------------------------------------

//***************************************************************************
// FFmpeg binary
//***************************************************************************

// Path of ffmpeg-ma binary (ffmpeg-ma package) in the directory of the executable, in its ffmpeg, ffmpeg/lib or ffmpeg/bin subdirectory
// (FFmpeg package next to the executable, same directories as the first ones for the libraries) or in PATH,
// else of ffmpeg binary in the directory of the executable or in its subdirectories
// Empty if not found (ffmpeg in PATH is used)
std::string FFmpeg_Binary();

//***************************************************************************
// FFmpeg libraries (libavcodec and libavutil), available if RAWCOOKED_FFMPEG is defined
// Loaded at run time on Windows and macOS (or if RAWCOOKED_FFMPEG_DYNAMIC is defined) so they are optional, linked on other platforms
// Functions of the libraries are in FFmpeg_Functions.h
//***************************************************************************

// Use this libavcodec (and the libavutil it uses) instead of searching for the libraries, if FileName is the name of a libavcodec library (false if not), before the libraries are loaded
bool FFmpeg_Library_SetPath(const std::string& FileName);

// Load the libraries if not already done, false if they are not available (Error is set to the reason)
bool FFmpeg_Library_Load(std::string* Error = nullptr);

// Indexes of the Vulkan devices to use if libraries are available: all discrete GPUs, else the first GPU, else the device used by default by FFmpeg if AllowCPU (a CPU implementation e.g. lavapipe), FFmpeg log is quiet after this call
// Not needed if Vulkan is explicitly requested, any Vulkan device is used in that case
std::vector<size_t> FFmpeg_Library_Vulkan(bool AllowCPU = false);

// Vulkan device explicitly requested and not available (e.g. no device with this index), known when the decoder which needs it creates it, (size_t)-1 if none
void FFmpeg_Library_SetVulkanMissing(size_t Device);
size_t FFmpeg_Library_VulkanMissing();

// Name of the Vulkan device used by default by FFmpeg (detected if not already done, the device is then kept for the decoder), empty if not available
// IsGPU: set to true if the device is a GPU, false if it is a CPU implementation (e.g. lavapipe) or if not available
std::string FFmpeg_Library_VulkanName(bool* IsGPU = nullptr);

//---------------------------------------------------------------------------
#endif
