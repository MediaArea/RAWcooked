/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
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
#include <string>
using namespace std;
//---------------------------------------------------------------------------

class rawcooked
{
public:
                                rawcooked();
                                ~rawcooked();

    bool                        Unique; // If set, data is for the whole stream (unique file)

    uint8_t*                    Before;
    uint64_t                    Before_Size;

    uint8_t*                    After;
    uint64_t                    After_Size;

    void                        Parse();
    void                        ResetTrack();
    void                        Close();

    string                      FileName;
    string                      FileNameDPX;

private:
    uint64_t                    Buffer_Offset;

    // File IO
    FILE*                       F;
    size_t                      BlockCount;
    void WriteToDisk(uint8_t* Buffer, size_t Buffer_Size);

    // First frame info
    uint8_t*                    FirstFrame_Before;
    size_t                      FirstFrame_Before_Size;
    uint8_t*                    FirstFrame_After;
    size_t                      FirstFrame_After_Size;
    uint8_t*                    FirstFrame_FileName;
    size_t                      FirstFrame_FileName_Size;
};

//---------------------------------------------------------------------------
#endif
