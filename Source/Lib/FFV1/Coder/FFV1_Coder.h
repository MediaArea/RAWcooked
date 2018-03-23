/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFV1_CoderH
#define FFV1_CoderH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Config.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// By FFV1 bitstream design
const size_t MAX_CONTEXT_INPUTS = 5; 
const size_t MAX_QUANT_TABLE_SET_INDEXES = 3;
const size_t MAX_QUANT_TABLE_SIZE = 256;

//---------------------------------------------------------------------------
// Hard-coded limit, should be changed to dynamic
const size_t MAX_QUANT_TABLES = 8;

//---------------------------------------------------------------------------
// quant_table_set_index
struct quant_table_set_index_struct
{
    uint32_t Index;
};
typedef quant_table_set_index_struct quant_table_set_indexes_struct[MAX_QUANT_TABLE_SET_INDEXES];

//---------------------------------------------------------------------------
// quant_table_set_struct
struct states_struct;
typedef pixel_t quant_table_struct[MAX_QUANT_TABLE_SIZE];
typedef quant_table_struct quant_tables_struct[MAX_CONTEXT_INPUTS];
class quant_table_set_struct
{
public:
    quant_tables_struct     QuantTables;
    size_t                  Contexts_Count;

    quant_table_set_struct() :
        Contexts_Count(0)
    {
    }
};
typedef quant_table_set_struct quant_table_sets_struct[MAX_QUANT_TABLES];

//---------------------------------------------------------------------------
// Base coder class
class coder_base
{
public:
    coder_base(size_t quant_table_index_count_, quant_table_sets_struct& Global_PerQuantTable_) :
        quant_table_set_index_count(quant_table_index_count_),
        QuantTableSets(Global_PerQuantTable_)
    {}
    virtual ~coder_base() {}

    virtual void                GOP_Init(quant_table_set_indexes_struct& quant_table_set_indexes) {}
    virtual void                Plane_Init() {}
    virtual void                Line_Init(size_t quant_table_set_index) {}
    virtual pixel_t             Sample_Delta(int32_t context_idx) = 0;

    // Status
    virtual bool                IsUnderrun() = 0;
    virtual size_t              BytesUsed() = 0;

protected:
    //Default (for GOP_Init)
    size_t                      quant_table_set_index_count;
    quant_table_sets_struct&    QuantTableSets;
};



//---------------------------------------------------------------------------
#endif
