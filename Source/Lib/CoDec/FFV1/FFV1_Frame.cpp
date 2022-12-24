/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "Lib/Utils/CRC32/ZenCRC32.h"
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreorder"
#endif
#include "ThreadPool.h"
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
//---------------------------------------------------------------------------

//***************************************************************************
// Threads
//***************************************************************************

int Frame_Thread(slice* Slice)
{
    Slice->Parse();
    return 1;
}

//***************************************************************************
// Info
//***************************************************************************

extern const state_transitions_struct default_state_transitions =
{
    {
      0,  0,  0,  0,  0,  0,  0,  0, 20, 21, 22, 23, 24, 25, 26, 27,
     28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 37, 38, 39, 40, 41, 42,
     43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 56, 57,
     58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
     74, 75, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88,
     89, 90, 91, 92, 93, 94, 94, 95, 96, 97, 98, 99,100,101,102,103,
    104,105,106,107,108,109,110,111,112,113,114,114,115,116,117,118,
    119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,133,
    134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,
    150,151,152,152,153,154,155,156,157,158,159,160,161,162,163,164,
    165,166,167,168,169,170,171,171,172,173,174,175,176,177,178,179,
    180,181,182,183,184,185,186,187,188,189,190,190,191,192,194,194,
    195,196,197,198,199,200,201,202,202,204,205,206,207,208,209,209,
    210,211,212,213,215,215,216,217,218,219,220,220,222,223,224,225,
    226,227,227,229,229,230,231,232,234,234,235,236,237,238,239,240,
    241,242,243,244,245,246,247,248,248,  0,  0,  0,  0,  0,  0,  0,
    }
};

static inline uint32_t BigEndian2int24u(const uint8_t* B)
{
    return (((uint32_t)B[0]) << 16) | (((uint32_t)B[1]) << 8) | B[2];
}

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
ffv1_frame::ffv1_frame(ThreadPool* Pool_) :
    // Decoded frame
    RawFrame(NULL),
    // Temp
    KeyFrame_IsPresent(false),
    Slices(NULL),
    Pool(Pool_)
{
    E.AssignStateTransitions(default_state_transitions);
}

//---------------------------------------------------------------------------
ffv1_frame::~ffv1_frame()
{
    Clear();
}

//***************************************************************************
// External metadata
//***************************************************************************

//---------------------------------------------------------------------------
void ffv1_frame::SetWidth(uint32_t width)
{
    P.width = width;
}

//---------------------------------------------------------------------------
void ffv1_frame::SetHeight(uint32_t height)
{
    P.height = height;
}

//***************************************************************************
// Before - Global
//***************************************************************************

//---------------------------------------------------------------------------
bool ffv1_frame::OutOfBand(const uint8_t* Buffer, size_t Buffer_Size)
{
    if (!Buffer_Size)
        return true;

    P.ConfigurationRecord_IsPresent = true;
    Clear();

    //Coherency tests
    if (Buffer_Size<4)
        return P.Error("FFV1-HEADER-END:1");
    if (ZenCRC32(Buffer, (size_t)Buffer_Size))
        return P.Error("FFV1-HEADER-configuration_record_crc_parity:1");

    E.AssignBuffer(Buffer, Buffer_Size-4);

    if (P.Parse(E, true))
        return true;

    // Slices
    Slices = new slice_struct[P.num_h_slices*P.num_v_slices];
    memset(Slices, 0x00, P.num_h_slices*P.num_v_slices * sizeof(slice_struct));

    // Reserved + configuration_record_crc_parity

    return true;
}

//---------------------------------------------------------------------------
bool ffv1_frame::Process(const uint8_t* Buffer, size_t Buffer_Size)
{
    if (!Slices)
    {
        if (P.ConfigurationRecord_IsPresent)
            return true; //There is a problem

        // No slice in this stream, fake 1 slice
        Slices = new slice_struct[1];
        memset(Slices, 0x00, sizeof(slice_struct));
    }

    E.AssignBuffer(Buffer, Buffer_Size);

    // keyframe
    uint8_t State = states_default;
    bool keyframe = E.b(State);
    if (P.ConfigurationRecord_IsPresent && P.intra && !keyframe)
        return P.Error("FFV1-FRAME-key_frame-ISNOTINTRA:1");
    if (keyframe)
        KeyFrame_IsPresent = true;
    if (!KeyFrame_IsPresent)
        return P.Error("FFV1-FRAME-key_frame-NOINFIRSTFRAME:1");

    size_t Slices_Size = 0;
    if (P.ConfigurationRecord_IsPresent)
    {
        if (P.num_h_slices >= P.width)
            return P.Error("FFV1-HEADER-num_h_slices:1");
        if (P.num_v_slices >= P.height)
            return P.Error("FFV1-HEADER-num_v_slices:1");

        size_t info = 0
        | ((P.bits_per_raw_sample - 1)          <<  0)
        | (P.chroma_planes                      <<  6)
        | (P.alpha_plane                        <<  7)
        | ((1 << P.log2_h_chroma_subsample)     <<  8)
        | ((1 << P.log2_v_chroma_subsample)     << 12)
        | (P.colorspace_type                    << 16)
        | ((P.num_h_slices - 1)                 << 24)
        ;
      RawFrame->Create(P.width, P.height, info);

        uint64_t Slices_BufferPos = Buffer_Size;
        while (Slices_BufferPos)
        {
            if (Slices_BufferPos < P.TailSize)
                return true; //There is a problem

            size_t Size = BigEndian2int24u(Buffer + (size_t)Slices_BufferPos - P.TailSize);
            Size += P.TailSize;

            if (Size > Slices_BufferPos)
                return true; //There is a problem
            Slices_BufferPos -= Size;

            Slices[Slices_Size].BufferSize = Size;
            slice_struct& Slice_Current = Slices[Slices_Size];
            slice*& Slice_Content = Slice_Current.Content;
            if (!Slice_Content)
                Slice_Content = new slice(&P);
            Slice_Content->Init(Buffer + Slices_BufferPos, Size, keyframe, !Slices_BufferPos, RawFrame);
            Slices_Size++;
        }
        Slices_Size--;
    }
    else
    {
        Slices_Size = 0;
        slice_struct& Slice_Current = Slices[0];
        Slice_Current.BufferSize = Buffer_Size;
        slice*& Slice_Content = Slice_Current.Content;
        if (!Slice_Content)
            Slice_Content = new slice(&P);
        Slice_Content->Init(Buffer, Buffer_Size, keyframe, true, RawFrame);
    }

    if (Pool)
    {
        std::vector<std::future<int>> Futures;
        for (size_t i = Slices_Size; i <= Slices_Size; i--)
            Futures.push_back(Pool->submit(Frame_Thread, Slices[i].Content));
        for (size_t i = Slices_Size; i <= Slices_Size; i--) // TODO: don't wait for the parsing of the frame before peeking the next frame
            Futures[i].get();
    }
    else
    {
        for (size_t i = Slices_Size; i <= Slices_Size; i--)
            Frame_Thread(Slices[i].Content);
    }

    RawFrame->Finalize(P.num_h_slices, P.num_v_slices);

    return false;
}

//***************************************************************************
// Errors
//***************************************************************************

//---------------------------------------------------------------------------
const char* ffv1_frame::ErrorMessage()
{
    return P.error_message;
}

//***************************************************************************
// Helpers
//***************************************************************************

//---------------------------------------------------------------------------
void ffv1_frame::Clear()
{
    if (Slices)
    {
        size_t Slices_Count = (P.ConfigurationRecord_IsPresent ? (P.num_h_slices*P.num_v_slices) : 1);
        for (size_t i = 0; i < Slices_Count; i++)
            delete Slices[i].Content;
        delete[] Slices;
    }
    Slices = nullptr;
}

