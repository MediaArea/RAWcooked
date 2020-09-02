/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef ZenCRC32H
#define ZenCRC32H
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

uint32_t ZenCRC32(const uint8_t* Buffer, size_t Size);

#endif
