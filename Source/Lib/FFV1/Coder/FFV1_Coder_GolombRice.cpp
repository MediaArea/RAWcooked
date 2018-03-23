/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */


//---------------------------------------------------------------------------
#include "Lib/FFV1/Coder/FFV1_Coder_GolombRice.h"
//---------------------------------------------------------------------------

//***************************************************************************
// Const
//***************************************************************************

// Coming from FFv1 spec.
static const uint8_t GR_log2_run[41] = {
    0 , 0, 0, 0, 1, 1, 1, 1,
    2 , 2, 2, 2, 3, 3, 3, 3,
    4 , 4, 5, 5, 6, 6, 7, 7,
    8 , 9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,
    24,
};

static const uint32_t GR_run[41] =
{
    1 << 0,
    1 << 0,
    1 << 0,
    1 << 0,
    1 << 1,
    1 << 1,
    1 << 1,
    1 << 1,
    1 << 2,
    1 << 2,
    1 << 2,
    1 << 2,
    1 << 3,
    1 << 3,
    1 << 3,
    1 << 3,
    1 << 4,
    1 << 4,
    1 << 5,
    1 << 5,
    1 << 6,
    1 << 6,
    1 << 7,
    1 << 7,
    1 << 8,
    1 << 9,
    1 << 10,
    1 << 11,
    1 << 12,
    1 << 13,
    1 << 14,
    1 << 15,
    1 << 16,
    1 << 17,
    1 << 18,
    1 << 19,
    1 << 20,
    1 << 21,
    1 << 22,
    1 << 23,
    1 << 24,
};

//***************************************************************************
// GR_Context
//***************************************************************************

class GR_Context
{
public:
    int32_t ContextCount;
    int32_t Sum_Absolute;
    int32_t Sum_Corrected;
    int32_t Corrected;

    GR_Context() :
        ContextCount(1),
        Sum_Absolute(4),
        Sum_Corrected(0),
        Corrected(0)
    {}

    inline void Update()
    {
        if (ContextCount == 128)
        {
            ContextCount  >>= 1;
            Sum_Absolute  >>= 1;
            Sum_Corrected >>= 1;
        }
        ContextCount++;

        if (Sum_Corrected <= -ContextCount)
        {
            if (Corrected > -128) // Can not be less than -128
                Corrected--;

            Sum_Corrected += ContextCount;
            if (Sum_Corrected <= -ContextCount)
                Sum_Corrected = 1 - ContextCount;
        }
        else if (Sum_Corrected > 0)
        {
            if (Corrected < 127) // Can not be more than 127
                Corrected++;

            if (Sum_Corrected > ContextCount)
                Sum_Corrected = 0;
            else
                Sum_Corrected -= ContextCount;
        }
    }
};

inline int32_t GR_Decode(BitStream* BS, uint32_t bits_max, uint8_t k)
{
    int32_t q = 0;
    while (BS->Remain() && !BS->GetB())
    {
        q++;
        if (q >= 12) // ESC (Escape)
        {
            int32_t v = 11 + BS->Get4(bits_max);
            return (v >> 1) ^ -(v & 1); // Unsigned to signed
        }
    }

    int32_t v = (q << k) | BS->Get4(k);
    return (v >> 1) ^ -(v & 1); // Unsigned to signed
}

//---------------------------------------------------------------------------
coder_golombrice::coder_golombrice(quant_table_sets_struct& QuantTableSets, size_t quant_table_set_index_count, uint32_t w_, uint32_t bits_max_) :
    coder_base(quant_table_set_index_count, QuantTableSets),
    w(w_),
    bits_max(bits_max_),
    run_index(0),
    run_mode(0)
{
    for (size_t i = 0; i < MAX_QUANT_TABLE_SET_INDEXES; ++i)
        GR_Contexts_PerQuantTableSetIndex[i] = NULL;

    bits_mask_neg = 1 << (bits_max - 1);
    bits_mask = bits_mask_neg - 1;
}

//---------------------------------------------------------------------------
coder_golombrice::~coder_golombrice()
{
    for (size_t i = 0; i < MAX_QUANT_TABLE_SET_INDEXES; i++)
        delete[] GR_Contexts_PerQuantTableSetIndex[i];
}

//---------------------------------------------------------------------------
static int32_t GR_Code(BitStream* BS, uint32_t bits_max, uint32_t bits_mask_neg, uint32_t bits_mask, GR_Context* GR_context)
{
    uint8_t k = 0;
    while ((GR_context->ContextCount << k) < GR_context->Sum_Absolute)
        k++;

    int32_t code = GR_Decode(BS, bits_max, k);

    int32_t M = 2 * GR_context->Sum_Corrected + GR_context->ContextCount;
    code = code ^ (M >> 31);

    GR_context->Sum_Corrected += code;
    GR_context->Sum_Absolute += code >= 0 ? code : -code;

    code += GR_context->Corrected;

    GR_context->Update();

    bool neg = code & bits_mask_neg;
    code &= bits_mask; // Remove neg bit
    if (neg)
        code -= bits_mask_neg;

    return code;
}

//---------------------------------------------------------------------------
void coder_golombrice::GOP_Init(quant_table_set_indexes_struct& quant_table_set_indexes)
{
    for (size_t i = 0; i < quant_table_set_index_count; ++i)
    {
        delete[] GR_Contexts_PerQuantTableSetIndex[i];
        GR_Contexts_PerQuantTableSetIndex[i] = new GR_Context[QuantTableSets[quant_table_set_indexes[i].Index].Contexts_Count];
    }
}

//---------------------------------------------------------------------------
void coder_golombrice::Frame_Init(uint8_t* Buffer, size_t Buffer_Size)
{
    BS.Attach(Buffer, Buffer_Size);
}

//---------------------------------------------------------------------------
void coder_golombrice::Plane_Init()
{
    run_index_init();
}

//---------------------------------------------------------------------------
void coder_golombrice::Line_Init(size_t quant_table_set_index)
{
    GR_Contexts_Current = GR_Contexts_PerQuantTableSetIndex[quant_table_set_index];

    x = 0;
    run_mode_init();
}

//---------------------------------------------------------------------------
pixel_t coder_golombrice::Sample_Delta(int32_t context_idx)
{
    int32_t s = Sample_Delta_Private(context_idx);
    x++;
    return s;
}

//---------------------------------------------------------------------------
int32_t coder_golombrice::Sample_Delta_Private(int32_t ContextIdx)
{
    if (!run_mode && ContextIdx)
        return GR_Code(&BS, bits_max, bits_mask_neg, bits_mask, &GR_Contexts_Current[ContextIdx]); // Golomb Rice mode

    if (!run_mode)
        run_mode++; // Run mode

    if (run_mode == 1 && run_segment_length == 0)
    {
        if (BS.GetB())
        {
            run_segment_length = GR_run[run_index];
            if (x + run_segment_length <= w) // Limited by width of the line
                run_index++;
            run_segment_length--;
            if (run_segment_length >= 0)
                return 0;
        }
        else
        {
            run_mode++;

            if (run_index)
            {
                uint8_t count = GR_log2_run[run_index];
                run_index--;
                if (count)
                {
                    run_segment_length = ((int32_t)BS.Get4(count)) - 1;
                    if (run_segment_length >= 0)
                        return 0;
                }
                else
                    run_segment_length = -1;
            }
            else
                run_segment_length = -1;
        }
    }
    else if (--run_segment_length >= 0)
        return 0;

    // New symbol
    run_mode_init();
    int32_t u = GR_Code(&BS, bits_max, bits_mask_neg, bits_mask, &GR_Contexts_Current[ContextIdx]);
    if (u >= 0)
        u++;
    return u;
}

bool coder_golombrice::IsUnderrun()
{
    return BS.BufferUnderRun;
}
size_t coder_golombrice::BytesUsed()
{
    return BS.Offset_Get();
}

void coder_golombrice::run_index_init()
{
    run_index = 0;
}
void coder_golombrice::run_mode_init()
{
    run_segment_length = 0;
    run_mode = 0;
}
