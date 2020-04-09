/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFV1_RangeCoderH
#define FFV1_RangeCoderH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstdint>
#include <cstring>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// By FFV1 bitstream design
const size_t state_transitions_struct_size = 256;
const size_t states_default = 128;
const size_t states_end = 129;
const size_t states_size = 32;

//---------------------------------------------------------------------------
struct states_struct
{
    uint8_t States[states_size];

    states_struct()
    {
    }

    states_struct(uint8_t Value)
    {
        memset(States, Value, states_size);
    }
};

struct state_transitions_struct
{
    uint8_t States[state_transitions_struct_size];
};

class rangecoder
{
public:
    rangecoder() {}
    rangecoder(const uint8_t* Buffer, size_t Buffer_Size, const state_transitions_struct& state_transitions);

    // Init
    void                        AssignBuffer(const uint8_t* Buffer, size_t Buffer_Size);
    void                        AssignStateTransitions(const state_transitions_struct& new_state_transitions);
    void                        ReduceBuffer(size_t Buffer_Size); //Adapt the buffer limit

    // Run
    bool                        b(states_struct& States) { return b(States.States[0]); }
    bool                        b(uint8_t& State); // For quick access to bool with only 1 State value
    uint32_t                    u(states_struct& States);
    int32_t                     s(states_struct& States);

    // Info
    size_t                      BytesUsed();
    bool                        IsUnderrun();

private:
    uint32_t                    Current;
    uint32_t                    Mask;

    const uint8_t*              Buffer_Beg;
    const uint8_t*              Buffer_Cur;
    const uint8_t*              Buffer_End;

    state_transitions_struct    zero_state;
    state_transitions_struct    one_state;

    bool                        NextByte();
    void                        ForceUnderrun();
};

#endif
