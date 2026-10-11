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
//---------------------------------------------------------------------------

//***************************************************************************
// FFmpeg libraries (libavcodec and libavutil), available if RAWCOOKED_FFMPEG is defined
// Loaded at run time on Windows and macOS (or if RAWCOOKED_FFMPEG_DYNAMIC is defined) so they are optional, linked on other platforms
// Functions of the libraries are in FFmpeg_Functions.h
//***************************************************************************

// Load the libraries if not already done, false if they are not available (Error is set to the reason)
bool FFmpeg_Library_Load(std::string* Error = nullptr);

// Libraries are available and the Vulkan device used by default by FFmpeg is a GPU, or any device if AllowCPU (also a CPU implementation e.g. lavapipe), FFmpeg log is quiet after this call
// Not needed if Vulkan is explicitly requested, any Vulkan device is used in that case
bool FFmpeg_Library_Vulkan(bool AllowCPU = false);

//---------------------------------------------------------------------------
#endif
