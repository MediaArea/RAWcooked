/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/FFV1/FFV1_RangeCoder.h"
#include "Lib/FFV1/Coder/FFV1_Coder_RangeCoder.h"
using namespace std;
//---------------------------------------------------------------------------

coder_rangecoder::coder_rangecoder(quant_table_sets_struct& QuantTableSets, size_t quant_table_set_index_count, quant_table_sets_rc_struct& RC_ContextSets_) :
    coder_base(quant_table_set_index_count, QuantTableSets),
    RC_ContextSets(RC_ContextSets_)
{
    for (size_t i = 0; i < MAX_QUANT_TABLE_SET_INDEXES; ++i)
    {
        RC_Contexts_PerQuantTableSetIndex[i] = NULL;
        RC_Contexts_PerQuantTableSetIndex_Max[i] = 0;
    }
}

void coder_rangecoder::GOP_Init(quant_table_set_indexes_struct& quant_table_set_indexes)
{
    for (size_t i = 0; i < quant_table_set_index_count; i++)
    {
        uint32_t Index = quant_table_set_indexes[i].Index;

        //Adapt
        if (QuantTableSets[Index].Contexts_Count > RC_Contexts_PerQuantTableSetIndex_Max[i])
        {
            RC_contexts RC_Contexts_old = RC_Contexts_PerQuantTableSetIndex[i];
            RC_Contexts_PerQuantTableSetIndex[i] = new states_struct[QuantTableSets[Index].Contexts_Count];
            if (RC_Contexts_old)
            {
                memcpy(RC_Contexts_PerQuantTableSetIndex[i], RC_Contexts_old, RC_Contexts_PerQuantTableSetIndex_Max[i] * sizeof(RC_contexts));
                delete[] RC_Contexts_old;
            }
            RC_Contexts_PerQuantTableSetIndex_Max[i] = QuantTableSets[Index].Contexts_Count;
        }

        // Copy
        for (size_t j = 0; j < QuantTableSets[Index].Contexts_Count; j++)
            RC_Contexts_PerQuantTableSetIndex[i][j] = RC_ContextSets[Index].RC_Contexts[j];
    }
}

void coder_rangecoder::Frame_Init(rangecoder* E_)
{
    E = E_;
}

coder_rangecoder::~coder_rangecoder()
{
    for (size_t i = 0; i < MAX_QUANT_TABLE_SET_INDEXES; i++)
        delete[] RC_Contexts_PerQuantTableSetIndex[i];
}

//---------------------------------------------------------------------------
void coder_rangecoder::Line_Init(size_t quant_table_set_index)
{
    RC_Contexts_Current = RC_Contexts_PerQuantTableSetIndex[quant_table_set_index];
}

//---------------------------------------------------------------------------
pixel_t coder_rangecoder::Sample_Delta(int32_t context_idx)
{
    return E->s(RC_Contexts_Current[context_idx]);
}

bool coder_rangecoder::IsUnderrun()
{
    return E->IsUnderrun();
}
size_t coder_rangecoder::BytesUsed()
{
    return E->BytesUsed();
}

//***************************************************************************
// quant_table_set_rc_struct
//***************************************************************************

quant_table_set_rc_struct::quant_table_set_rc_struct()
{
    RC_Contexts = NULL;
    RC_Contexts_Max = 0;
}

quant_table_set_rc_struct::~quant_table_set_rc_struct()
{
    delete[] RC_Contexts;
}

void quant_table_set_rc_struct::RC_Contexts_Adapt(size_t Contexts_Count)
{
    if (Contexts_Count <= RC_Contexts_Max)
        return;

    RC_contexts RC_Contexts_old = RC_Contexts;
    RC_Contexts = new states_struct[Contexts_Count];
    if (RC_Contexts_old)
    {
        memcpy(RC_Contexts, RC_Contexts_old, RC_Contexts_Max * sizeof(RC_contexts));
        delete[] RC_Contexts_old;
    }
    RC_Contexts_Max = Contexts_Count;
}
