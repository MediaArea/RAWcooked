/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
*
*  Use of this source code is governed by a MIT-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Defines the maximum bit size of a pixel
typedef int32_t pixel_t;

//---------------------------------------------------------------------------
struct write_to_disk_struct
{
    const char* FileName;
    bool        IsFirstFrame;
    const char* FileNameDPX;
};
