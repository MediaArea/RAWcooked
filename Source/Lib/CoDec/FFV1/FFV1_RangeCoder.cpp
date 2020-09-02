/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */


//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_RangeCoder.h"
using namespace std;
//---------------------------------------------------------------------------

#define min(a,b) (((a)<(b))?(a):(b))

//---------------------------------------------------------------------------
rangecoder::rangecoder(const uint8_t* Buffer, size_t Buffer_Size, const state_transitions_struct& state_transitions)
{
    AssignBuffer(Buffer, Buffer_Size);
    AssignStateTransitions(state_transitions);
}

void rangecoder::AssignBuffer(const uint8_t* Buffer, size_t Buffer_Size)
{
    // Assign buffer
    Buffer_Beg = Buffer;
    Buffer_Cur = Buffer;
    Buffer_End = Buffer + Buffer_Size;

    //Init
    if (Buffer_Size)
        Current = *Buffer_Cur;
    Mask = 0xFF;
    Buffer_Cur++;
}

void rangecoder::AssignStateTransitions(const state_transitions_struct& new_state_transitions)
{
    one_state = new_state_transitions;
    zero_state.States[0] = 0;
    for (size_t i = 1; i<state_transitions_struct_size; i++)
        zero_state.States[i] = -one_state.States[state_transitions_struct_size - i];
}

//---------------------------------------------------------------------------
void rangecoder::ReduceBuffer(size_t Buffer_Size)
{
    Buffer_End = Buffer_Beg + Buffer_Size;
}

//---------------------------------------------------------------------------
size_t rangecoder::BytesUsed()
{
    if (Buffer_Cur>Buffer_End)
        return Buffer_End - Buffer_Beg;
    return Buffer_Cur - Buffer_Beg - (Mask<0x100 ? 0 : 1);
}

//---------------------------------------------------------------------------
bool rangecoder::IsUnderrun()
{
    return (Buffer_Cur - (Mask<0x100 ? 0 : 1)>Buffer_End) ? true : false;
}

//---------------------------------------------------------------------------
bool rangecoder::NextByte()
{
    return false;
}

//---------------------------------------------------------------------------
bool rangecoder::b(uint8_t& State)
{
    // Next byte
    if (Mask<0x100)
    {
        Current <<= 8;

        // If more, underrun, we return 0
        // If equal, last byte assumed to be 0x00
        // If less, consume the next byte
        if (Buffer_Cur>Buffer_End)
            return false;
        if (Buffer_Cur<Buffer_End)
            Current |= *Buffer_Cur;

        Mask <<= 8;
        Buffer_Cur++;
    }

    //Range Coder boolean value computing
    uint32_t Mask2 = (Mask*State) >> 8;
    Mask -= Mask2;
    if (Current<Mask)
    {
        State = zero_state.States[State];
        return false;
    }
    Current -= Mask;
    Mask = Mask2;
    State = one_state.States[State];
    return true;
}

//---------------------------------------------------------------------------
uint32_t rangecoder::u(states_struct& States)
{
    if (b(States.States [0]))
        return 0;

    int e = 0;
    while (b(States.States [1 + min(e, 9)])) // 1..10
    {
        e++;
        if (e > 31)
        {
            ForceUnderrun(); // stream is buggy or unsupported, we disable it completely and we indicate that it is NOK
            return 0;
        }
    }

    uint32_t a = 1;
    int i = e - 1;
    while (i >= 0)
    {
        a <<= 1;
        if (b(States.States [ 22 + min(i, 9)]))  // 22..31
            ++a;
        i--;
    }

    return a;
}

//---------------------------------------------------------------------------
int32_t rangecoder::s(states_struct& States)
{

    // First version is rolled version, from specs
    // Second version is unrolled, need some advances benches but a first test shows 3-4% overal decode improvement with a complex 10-bit file

#if 0

    if (b(States.States[0]))
        return 0;

    int e = 0;
    while (b(States.States[1 + min(e, 9)])) // 1..10
    {
        e++;
        if (e > 31)
        {
            ForceUnderrun(); // stream is buggy or unsupported, we disable it completely and we indicate that it is NOK
            return 0;
        }
    }

    int32_t a = 1;
    int i = e - 1;
    while (i >= 0)
    {
        a <<= 1;
        if (b(States.States[22 + min(i, 9)]))  // 22..31
            ++a;
        i--;
    }

    if (b(States.States[11 + min(e, 10)])) // 11..21
        return -a;
    else
        return a;

#else

    if (b(States.States[0]))
        return 0;

    int e;
    int32_t a = 1;
    if (b(States.States[1])) // 1..10
    {
        if (b(States.States[2])) // 1..10
        {
            if (b(States.States[3])) // 1..10
            {
                if (b(States.States[4])) // 1..10
                {
                    if (b(States.States[5])) // 1..10
                    {
                        if (b(States.States[6])) // 1..10
                        {
                            if (b(States.States[7])) // 1..10
                            {
                                if (b(States.States[8])) // 1..10
                                {
                                    if (b(States.States[9])) // 1..10
                                    {
                                        e = 9;
                                        while (b(States.States[10])) // 1..10
                                        {
                                            e++;
                                            if (e > 31)
                                            {
                                                ForceUnderrun(); // stream is buggy or unsupported, we disable it completely and we indicate that it is NOK
                                                return 0;
                                            }
                                        }
                                        int i = e - 10;
                                        while (i >= 0)
                                        {
                                            a <<= 1;
                                            if (b(States.States[31]))  // 22..31
                                                ++a;
                                            i--;
                                        }
                                        a <<= 1;
                                        if (b(States.States[30]))  // 22..31
                                            ++a;
                                    }
                                    else
                                        e = 8;
                                    a <<= 1;
                                    if (b(States.States[29]))  // 22..31
                                        ++a;
                                }
                                else
                                    e = 7;
                                a <<= 1;
                                if (b(States.States[28]))  // 22..31
                                    ++a;
                            }
                            else
                                e = 6;
                            a <<= 1;
                            if (b(States.States[27]))  // 22..31
                                ++a;
                        }
                        else
                            e = 5;
                        a <<= 1;
                        if (b(States.States[26]))  // 22..31
                            ++a;
                    }
                    else
                        e = 4;
                    a <<= 1;
                    if (b(States.States[25]))  // 22..31
                        ++a;
                }
                else
                    e = 3;
                a <<= 1;
                if (b(States.States[24]))  // 22..31
                    ++a;
            }
            else
            {
                if (b(States.States[23]))  // 22..31
                    a = 6;
                else
                    a = 4;
                if (b(States.States[22]))  // 22..31
                    ++a;
                if (b(States.States[13])) // 11..21
                    return -a;
                else
                    return a;
            }
            a <<= 1;
            if (b(States.States[23]))  // 22..31
                ++a;
        }
        else
        {
            if (b(States.States[22]))  // 22..31
            {
                if (b(States.States[12])) // 11..21
                    return -3;
                else
                    return 3;
            }
            else
            {
                if (b(States.States[12])) // 11..21
                    return -2;
                else
                    return 2;
            }
        }
        a <<= 1;
        if (b(States.States[22]))  // 22..31
            ++a;
        if (b(States.States[11 + min(e, 10)])) // 11..21
            return -a;
        else
            return a;
    }
    else
    {
        if (b(States.States[11])) // 11..21
            return -1;
        else
            return 1;
    }
#endif
}

//---------------------------------------------------------------------------
void rangecoder::ForceUnderrun()
{
    Mask = 0;
    Buffer_Cur = Buffer_End + 1;
}
