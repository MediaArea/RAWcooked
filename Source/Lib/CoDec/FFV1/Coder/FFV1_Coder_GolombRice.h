/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFV1_Coder_GolombRiceH
#define FFV1_Coder_GolombRiceH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/Coder/FFV1_Coder.h"
#include "Lib/Utils/BitStream/BitStream.h"
//---------------------------------------------------------------------------

class GR_Context;

class coder_golombrice : public coder_base
{
public:
    coder_golombrice(quant_table_sets_struct& QuantTableSets, size_t quant_table_set_index_count, uint32_t w, uint32_t bits_max);
    ~coder_golombrice();

    void                        GOP_Init(quant_table_set_indexes_struct& quant_table_set_indexes);
    void                        Frame_Init(const uint8_t* Buffer, size_t Buffer_Size);
    void                        Plane_Init();
    void                        Line_Init(size_t quant_table_set_index);
    pixel_t                     Sample_Delta(int32_t context_idx);
    
    // Status
    bool                        IsUnderrun();
    size_t                      BytesUsed();

private:
    // Helpers
    int32_t                     Sample_Delta_Private(int32_t context_idx);
    void                        run_index_init();
    void                        run_mode_init();
    
    // Temp
    BitStream                   BS;
    GR_Context*                 GR_Contexts_Current;
    GR_Context*                 GR_Contexts_PerQuantTableSetIndex[MAX_QUANT_TABLE_SET_INDEXES];
    uint32_t                    w;
    size_t                      x;
    uint32_t                    bits_max;
    uint32_t                    bits_mask_neg;
    uint32_t                    bits_mask;
    uint32_t                    run_index;
    uint32_t                    run_mode;
    int32_t                     run_segment_length;
};


#endif
