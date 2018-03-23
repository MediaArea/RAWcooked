/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef RawCookedH
#define RawCookedH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

typedef void(*write_file_call)(uint8_t*, size_t, void*);

class rawcooked
{
public:
    rawcooked();

    bool                        Unique; // If set, data is for the whole stream (unique file)

    uint8_t*                    Before;
    uint64_t                    Before_Size;

    uint8_t*                    After;
    uint64_t                    After_Size;

    void Parse();

    write_file_call             WriteFileCall;
    void*                       WriteFileCall_Opaque;

private:
    uint64_t                    Buffer_Offset;
};

//---------------------------------------------------------------------------
#endif
