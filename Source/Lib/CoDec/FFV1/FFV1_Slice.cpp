/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Slice.h"
#include "Lib/Transform/Transform.h"
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "Lib/Utils/CRC32/ZenCRC32.h"
#include <algorithm>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
extern const state_transitions_struct default_state_transitions;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
static inline int32_t get_median_number(int32_t one, int32_t two, int32_t three)
{
    if (one > two)
    {
        // one > two > three
        if (two > three)
            return two;

        // three > one > two
        if (three > one)
            return one;
        // one > three > two
        return three;
    }

    // three > two > one
    if (three > two)
        return two;

    // two > one && two > three

    // two > three > one
    if (three > one)
        return three;
    return one;
}

//---------------------------------------------------------------------------
static inline pixel_t predict(pixel_t *current, pixel_t *current_top, bool is_overflow_16bit)
{
    pixel_t LeftTop, Top, Left;
    if (is_overflow_16bit)
    {
        LeftTop = (int16_t)current_top[-1];
        Top = (int16_t)current_top[0];
        Left = (int16_t)current[-1];
    }
    else
    {
        LeftTop = current_top[-1];
        Top = current_top[0];
        Left = current[-1];
    }

    return get_median_number(Left, Left + Top - LeftTop, Top);
}

//---------------------------------------------------------------------------
static inline pixel_t get_context_3(pixel_t quant_table[MAX_CONTEXT_INPUTS][MAX_QUANT_TABLE_SIZE], pixel_t *src, pixel_t *last)
{
    const int LT = last[-1];
    const int T = last[0];
    const int RT = last[1];
    const int L = src[-1];

    return quant_table[0][(L - LT) & 0xFF]
        + quant_table[1][(LT - T) & 0xFF]
        + quant_table[2][(T - RT) & 0xFF];
}
static inline pixel_t get_context_5(pixel_t quant_table[MAX_CONTEXT_INPUTS][MAX_QUANT_TABLE_SIZE], pixel_t *src, pixel_t *last)
{
    const int LT = last[-1];
    const int T = last[0];
    const int RT = last[1];
    const int L = src[-1];
    const int TT = src[0];
    const int LL = src[-2];
    return quant_table[0][(L - LT) & 0xFF]
        + quant_table[1][(LT - T) & 0xFF]
        + quant_table[2][(T - RT) & 0xFF]
        + quant_table[3][(LL - L) & 0xFF]
        + quant_table[4][(TT - T) & 0xFF];
}

//***************************************************************************
// Slice
//***************************************************************************

//---------------------------------------------------------------------------
slice::slice(parameters* P_) :
    P(P_),
    Coder(NULL)
{
}

//---------------------------------------------------------------------------
slice::~slice()
{
    delete Coder;
}

//---------------------------------------------------------------------------
bool slice::SliceHeader()
{
    states_struct States(states_default);

    uint32_t slice_x, slice_y, slice_width_minus1, slice_height_minus1;
    slice_x = E.u(States);
    if (slice_x >= P->num_h_slices)
    {
        P->Error("FFV1-SLICE-slice_xywh:1");

        return false;
    }

    slice_y = E.u(States);
    if (slice_y >= P->num_h_slices)
    {
        P->Error("FFV1-SLICE-slice_xywh:1");

        return false;
    }

    slice_width_minus1 = E.u(States);
    uint32_t slice_x2 = slice_x + slice_width_minus1 + 1; //right boundary
    if (slice_x2 > P->num_h_slices)
    {
        P->Error("FFV1-SLICE-slice_xywh:1");

        return false;
    }

    slice_height_minus1 = E.u(States);
    uint32_t slice_y2 = slice_y + slice_height_minus1 + 1; //bottom boundary
    if (slice_y2 > P->num_v_slices)
    {
        P->Error("FFV1-SLICE-slice_xywh:1");

        return false;
    }

    //Computing boundaries, being careful about how are computed boundaries when there is not an integral number for Width  / num_h_slices or Height / num_v_slices (the last slice has more pixels)
    x = slice_x  * P->width / P->num_h_slices;
    y = slice_y  * P->height / P->num_v_slices;
    w = slice_x2 * P->width / P->num_h_slices - x;
    h = slice_y2 * P->height / P->num_v_slices - y;

    for (uint8_t i = 0; i < P->quant_table_set_index_count; i++)
    {
        quant_table_set_indexes[i].Index = E.u(States);
        if (quant_table_set_indexes[i].Index >= P->quant_table_set_count)
        {
            P->Error("FFV1-SLICE-quant_table_index:1");

            return false;
        }
    }
    picture_structure = E.u(States);
    if (picture_structure>3)
        P->Error("FFV1-SLICE-picture_structure:1");
    sar_num = E.u(States);
    sar_den = E.u(States);
    //if ((sar_num && !sar_den) || (!sar_num && sar_den))
    //    P->Error("FFV1-SLICE-sar_den:1"); //TODO: check FFmpeg code, looks like it is set to 0/1 when unknown

    return true;
}

//---------------------------------------------------------------------------
void slice::GOP_Init()
{
    if (!Coder)
    {
        switch (P->coder_type)
        {
            case 0 : 
                    Coder = new coder_golombrice(P->QuantTableSets, P->quant_table_set_index_count, w, P->bits_max);
                    break;
            case 1 : 
                    Coder = new coder_rangecoder(P->QuantTableSets, P->quant_table_set_index_count, P->RC_ContextSets);
                    break;
            default:;
        }
    }

    Coder->GOP_Init(quant_table_set_indexes);
}

//---------------------------------------------------------------------------
void slice::Init(const uint8_t* Buffer_, size_t Buffer_Size_, bool keyframe_, bool IsFirstSlice_, raw_frame* RawFrame_)
{
    RawFrame = RawFrame_;
    Buffer = Buffer_;
    Buffer_Size = Buffer_Size_;
    keyframe = keyframe_;
    IsFirstSlice = IsFirstSlice_;
}

//---------------------------------------------------------------------------
bool slice::Parse()
{
    // RangeCoder reset
    E.AssignBuffer(Buffer, Buffer_Size);
    E.AssignStateTransitions(default_state_transitions);

    // Skip key frame
    if (IsFirstSlice)
    {
        uint8_t State = states_default;
        E.b(State); // keyframe
    }

    // Parameters
    if (!P->ConfigurationRecord_IsPresent)
    {
        if (keyframe)
        {
            if (P->Parse(E, false))
                return true;
        }

        //Frame
        //delete RawFrame;
        //RawFrame = new raw_frame;
        size_t info = 0
            | ((P->bits_per_raw_sample - 1)         <<  0)
            | (P->chroma_planes                     <<  6)
            | (P->alpha_plane                       <<  7)
            | ((1 << P->log2_h_chroma_subsample)    <<  8)
            | ((1 << P->log2_v_chroma_subsample)    << 12)
            | (P->colorspace_type                   << 16)
            | ((P->num_h_slices - 1)                << 24)
            ;
        RawFrame->Create(P->width, P->height, info);
    }

    // CRC check
    if (P->ec == 1 && ZenCRC32(Buffer, Buffer_Size))
        return P->Error("FFV1-SLICE-slice_crc_parity:1");

    // RangeCoder reset
    Buffer_Size -= P->TailSize;
    E.ReduceBuffer(Buffer_Size);
    if (P->RC_state_transitions_custom)
        E.AssignStateTransitions(*P->RC_state_transitions_custom);

    if (P->ConfigurationRecord_IsPresent)
    {
        SliceHeader();
    }
    else
    {
        x = 0;
        y = 0;
        w = P->width;
        h = P->height;
        picture_structure = (uint32_t)-1;
        sar_num = 0;
        sar_den = 0;
        for (size_t i = 0; i<P->quant_table_set_index_count; i++)
            quant_table_set_indexes[i].Index = 0;
    }

    if (keyframe)
        GOP_Init();

    size_t Buffer_Offset;
    switch (P->coder_type)
    {
        case 0 : 
                {
                uint8_t State = states_end;
                E.b(State);
                }

                Buffer_Offset = E.BytesUsed(); // Computing how many bytes where consumed by the range coder

                ((coder_golombrice*)Coder)->Frame_Init(Buffer + Buffer_Offset, Buffer_Size - Buffer_Offset);
                break;
        case 1 : 
                Buffer_Offset = 0;
                ((coder_rangecoder*)Coder)->Frame_Init(&E);
                break;
        default:;
    }

    Buffer_Offset += SliceContent();
    if (Buffer_Offset<Buffer_Size)
        P->Error("FFV1-SLICE-JUNK:1");

    //SliceFooter
    if (P->ConfigurationRecord_IsPresent)
    {
        //uint32_t slice_size = BigEndian2int24u(Before + (size_t)Buffer_Offset); // slice_size //TODO: test when in buggy stream parsing mode
        //if (Element_Offset_Begin + slice_size != Buffer_Offset)
        //    P->Error("FFV1-SLICE-slice_size:1");
        if (P->ec == 1)
        {
            Buffer_Offset += 3;
            uint8_t error_status = Buffer[Buffer_Offset]; // error_status
            if (error_status)
                return P->Error("FFV1-SLICE-error_status:1");
            Buffer_Offset += 5; // error_status + slice_crc_parity
        }
    }

    return false;
}

//---------------------------------------------------------------------------
size_t slice::SliceContent()
{
    switch (P->colorspace_type)
    {
        case 0 : 
                SliceContent_PlaneThenLine();
                break;
        case 1 : 
                SliceContent_LineThenPlane();
                break;
        default:;
    }

    switch (P->coder_type)
    {
        case 1 : 
                {
                uint8_t State = states_end;
                E.b(State);
                }
                break;
        default:;
    }

    if (Coder->IsUnderrun())
        P->Error("FFV1-SLICE-SliceContent:1");
    return Coder->BytesUsed();
}

//---------------------------------------------------------------------------
void slice::SliceContent_PlaneThenLine()
{
    pixel_t* SamplesBuffer = new pixel_t[2 * (w + 3)];
    memset(SamplesBuffer, 0, 2 * (w + 3) * sizeof(pixel_t));
    auto Transform = Transform_Init(RawFrame, pix_style::YUVA, P->bits_per_raw_sample, x, y, w, h);

    SliceContent_PlaneThenLine(Transform, SamplesBuffer, 0);
    if (P->chroma_planes)
    {
        uint32_t w_Save = w;
        uint32_t h_Save = h;

        w = w_Save >> P->log2_h_chroma_subsample;
        if (w_Save & ((1 << P->log2_h_chroma_subsample) - 1))
            w++; //Is ceil
        h = h_Save >> P->log2_v_chroma_subsample;
        if (h_Save & ((1 << P->log2_v_chroma_subsample) - 1))
            h++; //Is ceil
        SliceContent_PlaneThenLine(Transform, SamplesBuffer, 1);
        SliceContent_PlaneThenLine(Transform, SamplesBuffer, 1);
        }

    if (P->alpha_plane)
        SliceContent_PlaneThenLine(Transform, SamplesBuffer, 2);

    delete Transform;
    delete[] SamplesBuffer;
}

//---------------------------------------------------------------------------
void slice::SliceContent_PlaneThenLine(transform_base* Transform, pixel_t* SamplesBuffer, uint32_t pos)
{
    pixel_t* sample[2];
    sample[0] = SamplesBuffer + 2;
    sample[1] = sample[0] + w + 3;

    Coder->Plane_Init();

    for (size_t y = 0; y < h; y++)
    {
        swap(sample[0], sample[1]);

        sample[1][-1] = sample[0][0];
        sample[0][w] = sample[0][w - 1];

        Line(pos, sample);

        //Copy the line to the frame buffer
        Transform->From(sample[1]);
    }
}

//---------------------------------------------------------------------------
void slice::SliceContent_LineThenPlane()
{
    pixel_t* SamplesBuffer = new pixel_t[2 * P->plane_count * (w + 3)];
    memset(SamplesBuffer, 0, 2 * P->plane_count * (w + 3) * sizeof(pixel_t));
    auto Transform = Transform_Init(RawFrame, pix_style::RGBA, P->bits_per_raw_sample, x, y, w, h);

    Coder->Plane_Init();

    pixel_t *sample[4][2];
    for (size_t x = 0; x < P->plane_count; x++)
    {
        sample[x][0] = SamplesBuffer + 2 * x * (w + 3) + 2;
        sample[x][1] = sample[x][0] + w + 3;
    }
    for (size_t x = P->plane_count; x < 4; x++)
    {
        sample[x][0] = NULL;
        sample[x][1] = NULL;
    }

    for (size_t y = 0; y < h; y++)
    {
        for (size_t c = 0; c < P->plane_count; c++)
        {
            swap(sample[c][0], sample[c][1]);

            sample[c][1][-1] = sample[c][0][0];
            sample[c][0][w] = sample[c][0][w - 1];

            Line((c + 1) >> 1, sample[c]);
        }

        //Copy the line to the frame buffer
        Transform->From(sample[0][1], sample[1][1], sample[2][1], sample[3][1]);
    }

    delete Transform;
    delete[] SamplesBuffer;
}

//---------------------------------------------------------------------------
void slice::Line(size_t quant_table_set_index, pixel_t *sample[2])
{
    Coder->Line_Init(quant_table_set_index);

    quant_tables_struct& QuantTables = P->QuantTableSets[quant_table_set_indexes[quant_table_set_index].Index].QuantTables;
    pixel_t& bits_mask=P->bits_mask;
    bool Is5 = QuantTables[3][127] ? true : false;
    pixel_t* s0c = sample[0];
    pixel_t* s0e = s0c + w;
    pixel_t* s1c = sample[1];

    while (s0c<s0e)
    {
        pixel_t context_idx = Is5 ? get_context_5(QuantTables, s1c, s0c) : get_context_3(QuantTables, s1c, s0c);

        pixel_t Value = predict(s1c, s0c, P->IsOverflow16bit);
        if (context_idx >= 0)
            Value += Coder->Sample_Delta(context_idx);
        else
            Value -= Coder->Sample_Delta(-context_idx);
        *s1c = Value & bits_mask;

        s0c++;
        s1c++;
    }
}


