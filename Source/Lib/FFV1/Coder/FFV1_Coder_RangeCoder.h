/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFV1_Coder_RangeCoderH
#define FFV1_Coder_RangeCoderH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/FFV1/Coder/FFV1_Coder.h"
using namespace std;
//---------------------------------------------------------------------------

class rangecoder;
typedef states_struct* RC_contexts;

class quant_table_set_rc_struct
{
public:
    RC_contexts             RC_Contexts;
    size_t                  RC_Contexts_Max;

    quant_table_set_rc_struct();
    ~quant_table_set_rc_struct();

    void RC_Contexts_Adapt(size_t Contexts_Count);
};
typedef quant_table_set_rc_struct quant_table_sets_rc_struct[MAX_QUANT_TABLES];

class coder_rangecoder : public coder_base
{
public:
    coder_rangecoder(quant_table_sets_struct& QuantTableSets, size_t quant_table_set_index_count, quant_table_sets_rc_struct& RC_ContextSets);
    ~coder_rangecoder();

    void                        GOP_Init(quant_table_set_indexes_struct& quant_table_set_indexes);
    void                        Frame_Init(rangecoder* E);
    void                        Line_Init(size_t quant_table_set_index);
    pixel_t                     Sample_Delta(int32_t context_idx);

    // Status
    bool                        IsUnderrun();
    size_t                      BytesUsed();

private:
    rangecoder*      E;
    quant_table_sets_rc_struct& RC_ContextSets;
    RC_contexts                 RC_Contexts_Current;
    RC_contexts                 RC_Contexts_PerQuantTableSetIndex[MAX_QUANT_TABLE_SET_INDEXES];
    size_t                      RC_Contexts_PerQuantTableSetIndex_Max[MAX_QUANT_TABLE_SET_INDEXES];
};

#endif
