/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Parameters.h"
#include "Lib/CoDec/FFV1/FFV1_RangeCoder.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
extern const state_transitions_struct default_state_transitions;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
parameters::~parameters()
{
    delete RC_state_transitions_custom;
}

//---------------------------------------------------------------------------
bool parameters::Parse(rangecoder& E, bool ConfigurationRecord_IsPresent)
{
    //Parsing
    states_struct States(states_default);
    version = E.u(States);
    if (ConfigurationRecord_IsPresent && version <= 1)
        return Error("FFV1-HEADER-version-OUTOFBAND:1");
    if (!ConfigurationRecord_IsPresent && version> 1)
        return Error("FFV1-HEADER-version-OUTOFBAND:1");
    if (version == 2 || version > 3)
        return Error(version == 2 ? "FFV1-HEADER-version-EXPERIMENTAL:1" : "FFV1-HEADER-version-LATERVERSION:1");
    if (version >= 3)
        micro_version = E.u(States);
    if ((version == 3 && micro_version<4))
        return Error("FFV1-HEADER-micro_version-EXPERIMENTAL:1");
    coder_type = E.u(States);
    if (coder_type>2)
        return Error("FFV1-HEADER-coder_type:1");
    if (coder_type == 2) //Range coder with custom state transition table
    {
        delete[] RC_state_transitions_custom;
        RC_state_transitions_custom = new state_transitions_struct;
            
        for (size_t i = 1; i < state_transitions_struct_size; i++)
        {
            int32_t state_transition_delta = default_state_transitions.States[i] + E.s(States);
            if (state_transition_delta<0x00 || state_transition_delta>0xFF)
                return Error("FFV1-HEADER-state_transition_delta:1");
            RC_state_transitions_custom->States[i] = (uint8_t)state_transition_delta;
        }

        coder_type = 1; // Only difference between coder_type 1 and 2 is the presence of state transition table
    }

    colorspace_type = E.u(States);
    if (colorspace_type>1)
        return Error("FFV1-HEADER-colorspace_type:1");
    if (version)
    {
        bits_per_raw_sample = E.u(States);
        if (bits_per_raw_sample>64)
            return Error("FFV1-HEADER-bits_per_raw_sample:1");
        if (bits_per_raw_sample == 0)
            bits_per_raw_sample = 8; // Spec decision due to old encoders
    }
    else
    {
        bits_per_raw_sample = 8;
    }

    chroma_planes = E.b(States);
    log2_h_chroma_subsample = E.u(States);
    log2_v_chroma_subsample = E.u(States);
    alpha_plane = E.b(States);
    if (version>1)
    {
        num_h_slices = E.u(States);
        num_v_slices = E.u(States);
        num_h_slices++;
        num_v_slices++;
        quant_table_set_count = E.u(States);
        if (quant_table_set_count>8)
            return Error("FFV1-HEADER-quant_table_count:1");
    }
    else
    {
        num_h_slices = 1;
        num_v_slices = 1;
        quant_table_set_count = 1;
    }
    for (size_t i = 0; i<quant_table_set_count; i++)
        if (QuantizationTableSet(E, i))
            return true;

    for (size_t i = 0; i<quant_table_set_count; i++)
    {
        quant_table_set_struct& QuantTableSet = QuantTableSets[i];
        size_t jMax = QuantTableSet.Contexts_Count;

        bool states_coded = false;
        if (version >= 3)
            states_coded = E.b(States);

        switch (coder_type)
        {
            case 1 :
                    {
                        quant_table_set_rc_struct& RC_Contexts = RC_ContextSets[i];
                        if (states_coded)
                        {
                            for (size_t j = 0; j < jMax; j++)
                                for (size_t k = 0; k < states_size; k++)
                                    RC_Contexts.RC_Contexts[j].States[k] = E.s(States);
                        }
                        else
                        {
                            for (size_t j = 0; j < jMax; j++)
                                memset(RC_Contexts.RC_Contexts[j].States, states_default, states_size);
                        }
                    }
                    break;
            default:
                    if (states_coded)
                    {
                        for (size_t j = 0; j < jMax; j++)
                            for (size_t k = 0; k < states_size; k++)
                                E.s(States); // if coder_type is not 1, can be ignored for the moment, usefulness not yet found (why is it there?)
                    }
        }
    }

    if (version >= 3)
    {
        ec = E.u(States);
        switch (ec)
        {
            case 0 : TailSize = 3; break;
            case 1 : TailSize = 8; break;
            default: return Error("FFV1-HEADER-ec:1");
        }
        if (micro_version)
        {
            intra = E.u(States);
            if (intra>1)
                return Error("FFV1-HEADER-intra:1");
        }
        else
            intra = 0;
    }
    else
    {
        ec = 0;
        intra = 0;
        TailSize = 0;
    }

    // Marking handling of 16-bit overflow computing
    IsOverflow16bit = (colorspace_type == 0 && bits_per_raw_sample == 16 && (coder_type == 1)) ? true : false; //TODO: check in FFmpeg the version when the stream is fixed. Note: only with YUV colorspace

    // Temp
    TailSize = (version >= 3) ? 3 : 0;
    if (ec)
        TailSize += 5;
    switch (colorspace_type)
    {
        case 0 : 
                plane_count = 1 + (chroma_planes ? 2 : 0) + (alpha_plane ? 1 : 0);
                quant_table_set_index_count = 1 + ((version < 4 || chroma_planes) ? 1 : 0) + (alpha_plane ? 1 : 0);
                bits_max = (bits_per_raw_sample <= 8) ? 8 : bits_per_raw_sample;
                break;
        case 1 : 
                plane_count = alpha_plane ? 4 : 3;
                quant_table_set_index_count = plane_count - 1;
                bits_max = bits_per_raw_sample + 1;
                break;
        default:;
    }
    bits_mask = (1 << bits_max) - 1;

    return false;
}

//---------------------------------------------------------------------------
bool parameters::Error(const char* Error)
{
    const char* expected = nullptr;
    error_message.compare_exchange_strong(
        expected,
        Error,
        std::memory_order_release,
        std::memory_order_relaxed
    );

    return true;
}

//---------------------------------------------------------------------------
const char* parameters::Error() const
{
    return error_message.load(std::memory_order_acquire);
}

//---------------------------------------------------------------------------
bool parameters::QuantizationTableSet(rangecoder& E, size_t i)
{
    pixel_t scale = 1;

    for (size_t j = 0; j < 5; j++)
        if (QuantizationTable(E, i, j, scale))
            return true;

    QuantTableSets[i].Contexts_Count = (scale + 1) >> 1;
    if (coder_type == 1)
        RC_ContextSets[i].RC_Contexts_Adapt(QuantTableSets[i].Contexts_Count);

    return false;
}

//---------------------------------------------------------------------------
bool parameters::QuantizationTable(rangecoder& E, size_t i, size_t j, pixel_t &scale)
{
    states_struct States(states_default);

    pixel_t v = 0;
    for (size_t k = 0; k < MAX_QUANT_TABLE_SIZE/2;)
    {
        uint32_t len_minus1;
        len_minus1 = E.u(States); // len_minus1");

        if (k + len_minus1 >= MAX_QUANT_TABLE_SIZE/2)
            return Error("FFV1-HEADER-QuantizationTable-len:1");

        for (uint32_t a = 0; a <= len_minus1; a++)
        {
            QuantTableSets[i].QuantTables[j][k] = scale * v;
            k++;
        }

        v++;
    }

    for (int k = 1; k < MAX_QUANT_TABLE_SIZE/2; k++)
        QuantTableSets[i].QuantTables[j][MAX_QUANT_TABLE_SIZE - k] = -QuantTableSets[i].QuantTables[j][k];
    QuantTableSets[i].QuantTables[j][MAX_QUANT_TABLE_SIZE/2] = -QuantTableSets[i].QuantTables[j][127];

    scale *= 2 * v - 1;
    if (scale > 32768)
        return Error("FFV1-HEADER-QuantizationTable-scale:1");

    return false;
}
