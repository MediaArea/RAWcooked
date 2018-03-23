/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstdint>
#include <cstddef>
#include <string>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Defines the maximum bit size of a pixel
typedef int32_t pixel_t;

//---------------------------------------------------------------------------
struct write_to_disk_struct
{
    const char* FileName;
    bool        IsFirstFile;
    bool        IsFirstFrame;
    string      FileNameDPX;

    // Actually RAWcooked data. TODO: move it in a specific structure
    uint8_t*    FirstFrame_Before;
    size_t      FirstFrame_Before_Size;
    uint8_t*    FirstFrame_After;
    size_t      FirstFrame_After_Size;
    uint8_t*    FirstFrame_FileName;
    size_t      FirstFrame_FileName_Size;
};
