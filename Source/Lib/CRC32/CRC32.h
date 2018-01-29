/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef CRC32H
#define CRC32H
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

uint32_t CRC32(const uint8_t* Buffer, size_t Size);

#endif
