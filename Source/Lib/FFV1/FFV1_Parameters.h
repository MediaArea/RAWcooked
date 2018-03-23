/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFV1_ParametersH
#define FFV1_ParametersH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/FFV1/FFV1_RangeCoder.h"
#include "Lib/FFV1/Coder/FFV1_Coder_RangeCoder.h"
#include "Lib/FFV1/Coder/FFV1_Coder.h"
#include <cstring>
using namespace std;
//---------------------------------------------------------------------------

class slice;

// Struct for others
struct slice_struct
{
    slice*                  Content;
    size_t                  BufferSize;
};

struct parameters
{
    parameters();
    ~parameters();

    // Run
    bool                        Parse(rangecoder& E, bool ConfigurationRecord_IsPresent);
    bool                        Error(const char* Error) { error_message = Error; return true; }

    // Common content
    uint32_t                    version;
    uint32_t                    micro_version;
    uint32_t                    coder_type;
    uint32_t                    colorspace_type;
    uint32_t                    bits_per_raw_sample;
    bool                        chroma_planes;
    uint32_t                    log2_h_chroma_subsample;
    uint32_t                    log2_v_chroma_subsample;
    bool                        alpha_plane;
    uint32_t                    num_h_slices;
    uint32_t                    num_v_slices;
    uint32_t                    quant_table_set_count;
    quant_table_sets_struct     QuantTableSets;
    uint32_t                    ec;
    uint32_t                    intra;
    uint32_t                    width;
    uint32_t                    height;

    // Specific to Range Coder coder_type
    state_transitions_struct*   RC_state_transitions_custom; // From state_transition_delta
    quant_table_sets_rc_struct  RC_ContextSets; // states before starting a slice

    // Temp (used for having the code readable)
    size_t                      TailSize;
    size_t                      quant_table_set_index_count;
    size_t                      plane_count;
    uint8_t                     bits_max;
    pixel_t                     bits_mask;
    bool                        ConfigurationRecord_IsPresent;
    bool                        IsOverflow16bit;

    // Error message
    const char*                 error_message;

private:
    bool                        QuantizationTableSet(rangecoder& E, size_t i);
    bool                        QuantizationTable(rangecoder& E, size_t i, size_t j, pixel_t &scale);
};


//---------------------------------------------------------------------------
#endif
