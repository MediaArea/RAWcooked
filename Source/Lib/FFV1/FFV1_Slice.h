/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFV1_SliceH
#define FFV1_SliceH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/FFV1/FFV1_Parameters.h"
#include "Lib/FFV1/Coder/FFV1_Coder_RangeCoder.h"
#include "Lib/FFV1/Coder/FFV1_Coder_GolombRice.h"
//---------------------------------------------------------------------------

//***************************************************************************
// Class Slice
//***************************************************************************

class raw_frame;

class slice
{
public:
    slice(parameters* P);
    ~slice();

    // Content
    void                        Init(uint8_t* Buffer, size_t Buffer_Size, bool keyframe, bool IsFirstSlice, raw_frame* RawFrame);
    void                        Parse();

    // Metadata (no impact on decoding)
    uint32_t                    picture_structure;
    uint32_t                    sar_num;
    uint32_t                    sar_den;

private:
    //Location of content in frame
    uint32_t                    x; // From slice_x
    uint32_t                    y; // From slice_y
    uint32_t                    w; // From slice_width
    uint32_t                    h; // From slice_height

    // Slice related content
    quant_table_set_indexes_struct quant_table_set_indexes;

    // Common
    parameters*                 P;
    rangecoder                  E;
    raw_frame*                  RawFrame;

    // Cache (useful only for splitiing init from parsing)
    uint8_t*                    Buffer;
    size_t                      Buffer_Size;
    bool                        keyframe;
    bool                        IsFirstSlice;

    //Helpers
    void                        GOP_Init();
    bool                        SliceHeader();
    size_t                      SliceContent();
    void                        SliceContent_PlaneThenLine();
    void                        SliceContent_PlaneThenLine(pixel_t* SamplesBuffer, uint32_t pos);
    void                        SliceContent_LineThenPlane();
    void                        Line(size_t quant_table_set_index, pixel_t *sample[2]);

    // Coder
    coder_base*                 Coder;
};

//---------------------------------------------------------------------------
#endif
