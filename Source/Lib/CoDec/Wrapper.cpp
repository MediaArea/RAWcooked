/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/CoDec/Wrapper.h"
#include "Lib/CoDec/FFmpeg_Library.h"
#include "Lib/CoDec/FFV1/FFV1_Frame_FFmpeg.h"
#include "Lib/CoDec/FFV1/FFV1_Frame_Hybrid.h"
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include "Lib/Uncompressed/EXR/EXR.h"
#include "Lib/Uncompressed/WAV/WAV.h"
#include "Lib/Uncompressed/AIFF/AIFF.h"
#include "Lib/Uncompressed/HashSum/HashSum.h"
#include "FLAC/stream_decoder.h"
#include <cstring>
#include <iostream>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
static format_kind FormatKind_[] =
{
    format_kind::unknown,
    format_kind::video,
    format_kind::audio,
    format_kind::audio,
    format_kind::unknown,
};
static_assert(format_Max == sizeof(FormatKind_) / sizeof(format_kind), IncoherencyMessage);
format_kind FormatKind(format Format)
{
    return FormatKind_[(size_t)Format];
}

//---------------------------------------------------------------------------
struct codecid_mapping
{
    const char* CodecID;
    format Format;
};
static codecid_mapping Format_CodecID_[] =
{
    { "A_FLAC", format::FLAC},
    { "A_PCM/FLOAT/IEEE", format::PCM},
    { "A_PCM/INT/BIG", format::PCM},
    { "A_PCM/INT/LIT", format::PCM},
    { "V_FFV1", format::FFV1},
    { "V_MS/VFW/FOURCC", format::VFW},
};
format Format_FromCodecID(const char* Name)
{
    for (const auto& Item : Format_CodecID_)
        if (!strcmp(Name, Item.CodecID))
            return Item.Format;
    return format::None;
}

//---------------------------------------------------------------------------
void audio_wrapper::SetConfig(uint8_t BitDepth, sign Sign, endianness Endianness)
{
    OutputBitDepth = BitDepth;
    if (Sign == sign::F)
        SignOrEndianess.Sign = sign::S;
    else if (OutputBitDepth <= 8)
        SignOrEndianess.Sign = Sign;
    else
        SignOrEndianess.Endianness = Endianness;
}

//---------------------------------------------------------------------------
// Frame is ffv1_frame (our decoder) or ffv1_frame_hybrid (FFmpeg decoders, alone or in parallel with our decoder)
template<typename frame>
class ffv1_wrapper : public video_wrapper
{
public:
    //Constructor/Destructor
    ffv1_wrapper(frame* Ffv1Frame);
    ~ffv1_wrapper();

    // Config
    void                        SetWidth(uint32_t Width);
    void                        SetHeight(uint32_t Height);

    // Actions
    bool                        OutOfBand(const uint8_t* Data, size_t Size);

protected:
    frame*                      Ffv1Frame;
};

//---------------------------------------------------------------------------
template<typename frame>
ffv1_wrapper<frame>::ffv1_wrapper(frame* Ffv1Frame_) :
    Ffv1Frame(Ffv1Frame_)
{
}

//---------------------------------------------------------------------------
template<typename frame>
ffv1_wrapper<frame>::~ffv1_wrapper()
{
    delete Ffv1Frame;
}

//---------------------------------------------------------------------------
template<typename frame>
void ffv1_wrapper<frame>::SetWidth(uint32_t Width)
{
    Ffv1Frame->SetWidth(Width);
}

//---------------------------------------------------------------------------
template<typename frame>
void ffv1_wrapper<frame>::SetHeight(uint32_t Height)
{
    Ffv1Frame->SetHeight(Height);
}

//---------------------------------------------------------------------------
template<typename frame>
bool ffv1_wrapper<frame>::OutOfBand(const uint8_t* Data, size_t Size)
{
    Ffv1Frame->RawFrame = RawFrame;
    return Ffv1Frame->OutOfBand(Data, Size);
}

//---------------------------------------------------------------------------
// Our decoder in 2 steps: decoding to a frame given by the caller (the end of the decoding is during the decoding of the next frame), then output by the caller (maybe in another thread)
class ffv1_split_wrapper : public ffv1_wrapper<ffv1_frame>
{
public:
    using ffv1_wrapper<ffv1_frame>::ffv1_wrapper;

    // Actions
    bool                        CanSplit() { return true; }
    void                        Decode(const uint8_t* Data, size_t Size, data_holder* Holder, raw_frame* Frame, void* Opaque);
    size_t                      Depth() { return 5; } // Outputs of up to 3 frames in parallel during the decoding of next frames
};

//---------------------------------------------------------------------------
void ffv1_split_wrapper::Decode(const uint8_t* Data, size_t Size, data_holder* Holder, raw_frame* Frame, void* Opaque)
{
    // Frame data kept until the end of the decoding
    if (Holder)
        Data = Holder->Hold();
    Ffv1Frame->RawFrame = Frame;
    auto Ready = FrameReady;
    Ffv1Frame->Process(Data, Size, [Ready, Holder, Opaque](bool Error)
    {
        if (Holder)
            Holder->Release();
        Ready->Ready(Opaque, Error);
    });
    if (!Holder)
        Ffv1Frame->Wait(); // Frame data is valid only during the call
}

#ifdef RAWCOOKED_FFMPEG
//---------------------------------------------------------------------------
// Our decoder and/or FFmpeg decoders, each decoder in its own thread
class ffv1_hybrid_wrapper : public ffv1_wrapper<ffv1_frame_hybrid>
{
public:
    using ffv1_wrapper<ffv1_frame_hybrid>::ffv1_wrapper;

    // Config
    void                        SetWidth(uint32_t Width) { Ffv1Frame->RawFrame = RawFrame; ffv1_wrapper<ffv1_frame_hybrid>::SetWidth(Width); } // Raw frame is set before the decoding threads use it

    // Actions
    bool                        CanSplit() { return true; }
    void                        Decode(const uint8_t* Data, size_t Size, data_holder* Holder, raw_frame* Frame, void* Opaque) { Ffv1Frame->FrameReady = FrameReady; Ffv1Frame->Remaining = Remaining; Ffv1Frame->Decode(Data, Size, Holder, Frame, Opaque); }
    size_t                      Padding() { return ffv1_frame_hybrid::Padding(); }
    size_t                      Depth() { return Ffv1Frame->Depth(); }
    void                        End() { Ffv1Frame->End(); }
};
#endif //RAWCOOKED_FFMPEG

//---------------------------------------------------------------------------
class flac_wrapper : public audio_wrapper
{
public:
    flac_wrapper();
    ~flac_wrapper();

    // Actions
    bool                        OutOfBand(const uint8_t* Data, size_t Size) { return Process(Data, Size, nullptr); }
    bool                        Process(const uint8_t* Data, size_t Size, data_holder* Holder);

    // libFLAC related helping functions
    void                        FLAC_Read(uint8_t buffer[], size_t* bytes);
    void                        FLAC_Tell(uint64_t* absolute_byte_offset);
    void                        FLAC_Metadata(uint8_t channels, uint8_t bits_per_sample);
    void                        FLAC_Write(const uint32_t* const buffer[], size_t blocksize);

private:
    FLAC__uint64                absolute_byte_offset_ = 0;
    FLAC__StreamDecoder*        Decoder_;
    const uint8_t*              Data_;
    size_t                      Size_;
    uint8_t                     channels_ = 0;
    uint8_t                     bits_per_sample_ = 0;
};

//---------------------------------------------------------------------------
FLAC__StreamDecoderReadStatus flac_read_callback(const FLAC__StreamDecoder*, FLAC__byte buffer[], size_t* bytes, void* client_data)
{
    flac_wrapper* D = (flac_wrapper*)client_data;
    D->FLAC_Read(buffer, bytes);
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}
FLAC__StreamDecoderTellStatus flac_tell_callback(const FLAC__StreamDecoder*, FLAC__uint64* absolute_byte_offset, void* client_data)
{
    flac_wrapper* D = (flac_wrapper*)client_data;
    D->FLAC_Tell(absolute_byte_offset);
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}
void flac_metadata_callback(const FLAC__StreamDecoder*, const FLAC__StreamMetadata* metadata, void* client_data)
{
    flac_wrapper* D = (flac_wrapper*)client_data;
    D->FLAC_Metadata(metadata->data.stream_info.channels, metadata->data.stream_info.bits_per_sample);
}
FLAC__StreamDecoderWriteStatus flac_write_callback(const FLAC__StreamDecoder* /*decoder*/, const FLAC__Frame* frame, const FLAC__int32* const buffer[], void* client_data)
{
    flac_wrapper* D = (flac_wrapper*)client_data;
    D->FLAC_Write((const uint32_t* const*)buffer, frame->header.blocksize);
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
void flac_error_callback(const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus status, void*)
{
    fprintf(stderr, "Got FLAC error : %s\n", FLAC__StreamDecoderErrorStatusString[status]); // Not expected, should be better handled
}

//---------------------------------------------------------------------------
flac_wrapper::flac_wrapper()
{
    Decoder_ = FLAC__stream_decoder_new();
    FLAC__stream_decoder_set_md5_checking(Decoder_, true);
    if (FLAC__stream_decoder_init_stream(Decoder_, flac_read_callback, 0, flac_tell_callback, 0, 0, flac_write_callback, flac_metadata_callback, flac_error_callback, this) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
    {
        FLAC__stream_decoder_delete(Decoder_);
        Decoder_ = nullptr;
        return;
    }
}

//---------------------------------------------------------------------------
flac_wrapper::~flac_wrapper()
{
    FLAC__stream_decoder_delete(Decoder_);
}

bool flac_wrapper::Process(const uint8_t* Data, size_t Size, data_holder* /*Holder*/)
{
    Data_ = Data;
    Size_ = Size;

    for (;;)
    {
        if (!FLAC__stream_decoder_process_single(Decoder_))
            return true;
        FLAC__uint64 Pos;
        if (!FLAC__stream_decoder_get_decode_position(Decoder_, &Pos))
            return true;
        if (Pos == absolute_byte_offset_)
            return false;
    }
}

//---------------------------------------------------------------------------
void flac_wrapper::FLAC_Read(uint8_t buffer[], size_t* bytes)
{
    if (*bytes > Size_)
        *bytes = Size_;
    memcpy(buffer, Data_, *bytes);
    Data_ += *bytes;
    Size_ -= *bytes;
    absolute_byte_offset_ += *bytes;
}

//---------------------------------------------------------------------------
void flac_wrapper::FLAC_Tell(uint64_t* absolute_byte_offset)
{
    *absolute_byte_offset = absolute_byte_offset_;
}

//---------------------------------------------------------------------------
void flac_wrapper::FLAC_Metadata(uint8_t channels, uint8_t bits_per_sample)
{
    channels_ = channels;
    OutputBitDepth = bits_per_sample; // Value can be modified later by container information
    bits_per_sample_ = bits_per_sample;
}

//---------------------------------------------------------------------------
void flac_wrapper::FLAC_Write(const uint32_t* const buffer[], size_t blocksize)
{
    auto& Buffer = RawFrame->Buffer();
    if (!Buffer.Data())
        Buffer.Create(16384 / 8 * OutputBitDepth * channels_); // 16384 is the max blocksize in spec
    auto Data = Buffer.DataForModification();

    // Converting libFLAC output to WAV style
    uint8_t channels = channels_;
    switch (OutputBitDepth)
    {
    case 8:
        switch (bits_per_sample_)
        {
        case 8:
           switch (SignOrEndianess.Sign)
           {
           case sign::S:
           case sign::F:
               for (size_t i = 0; i < blocksize; i++)
                   for (size_t j = 0; j < channels; j++)
                   {
                       *(Data++) = (uint8_t)(buffer[j][i]);
                   }
               break;
           case sign::U:
               for (size_t i = 0; i < blocksize; i++)
                   for (size_t j = 0; j < channels; j++)
                   {
                       *(Data++) = (uint8_t)((buffer[j][i]) + 128);
                   }
               break;
           }
           break;
        case 16:
           switch (SignOrEndianess.Sign)
           {
           case sign::S:
           case sign::F:
               for (size_t i = 0; i < blocksize; i++)
                   for (size_t j = 0; j < channels; j++)
                   {
                       *(Data++) = (uint8_t)(buffer[j][i] >> 8);
                   }
               break;
           case sign::U:
               for (size_t i = 0; i < blocksize; i++)
                   for (size_t j = 0; j < channels; j++)
                   {
                       *(Data++) = (uint8_t)((buffer[j][i] >> 8) + 128);
                   }
               break;
           }
           break;
        }
        break;
    case 16:
        switch (SignOrEndianess.Endianness)
        {
        case endianness::BE:
            for (size_t i = 0; i < blocksize; i++)
                for (size_t j = 0; j < channels; j++)
                {
                    *(Data++) = (uint8_t)(buffer[j][i] >> 8);
                    *(Data++) = (uint8_t)(buffer[j][i]);
                }
            break;
        case endianness::LE:
            for (size_t i = 0; i < blocksize; i++)
                for (size_t j = 0; j < channels; j++)
                {
                    *(Data++) = (uint8_t)(buffer[j][i]);
                    *(Data++) = (uint8_t)(buffer[j][i] >> 8);
                }
            break;
        }
        break;
    case 24:
        switch (SignOrEndianess.Endianness)
        {
        case endianness::BE:
            for (size_t i = 0; i < blocksize; i++)
                for (size_t j = 0; j < channels; j++)
                {
                    *(Data++) = (uint8_t)(buffer[j][i] >> 16);
                    *(Data++) = (uint8_t)(buffer[j][i] >> 8);
                    *(Data++) = (uint8_t)(buffer[j][i]);
                }
            break;
        case endianness::LE:
            for (size_t i = 0; i < blocksize; i++)
                for (size_t j = 0; j < channels; j++)
                {
                    *(Data++) = (uint8_t)(buffer[j][i]);
                    *(Data++) = (uint8_t)(buffer[j][i] >> 8);
                    *(Data++) = (uint8_t)(buffer[j][i] >> 16);
                }
            break;
        }
        break;
    case 32:
        switch (SignOrEndianess.Endianness)
        {
        case endianness::BE:
            for (size_t i = 0; i < blocksize; i++)
                for (size_t j = 0; j < channels; j++)
                {
                    *(Data++) = (uint8_t)(buffer[j][i] >> 16);
                    *(Data++) = (uint8_t)(buffer[j][i] >> 8);
                    *(Data++) = (uint8_t)(buffer[j][i]);
                }
            break;
        case endianness::LE:
            for (size_t i = 0; i < blocksize; i++)
                for (size_t j = 0; j < channels; j++)
                {
                    *(Data++) = (uint8_t)(buffer[j][i]);
                    *(Data++) = (uint8_t)(buffer[j][i] >> 8);
                    *(Data++) = (uint8_t)(buffer[j][i] >> 16);
                }
            break;
        }
        break;
    }

    Buffer.Resize(Data - Buffer.Data());

    RawFrame->Process();
}

//---------------------------------------------------------------------------
class pcm_wrapper : public audio_wrapper
{
public:
    // Actions
    bool                        Process(const uint8_t* Data, size_t Size, data_holder* Holder);
};

//---------------------------------------------------------------------------
bool pcm_wrapper::Process(const uint8_t* Data, size_t Size, data_holder* /*Holder*/)
{
    RawFrame->AssignBufferView(Data, Size);
    return RawFrame->Process();
}

//---------------------------------------------------------------------------
#define CREATEWRAPPER(_NAME, _FLAVOR) case format::_FLAVOR: return new _NAME##_wrapper();
base_wrapper* CreateWrapper(format Format, ThreadPool* Pool, bool UseInternalDecoder, bool UseFFmpegDecoder, bool UseVulkanDecoder, bool UseAutoDecoder, bool AllowVulkanCPU)
{
    switch (Format)
    {
    case format::FFV1:
        if (UseAutoDecoder)
        {
            // Our decoder and FFmpeg Vulkan decoder in parallel if a GPU (or a CPU implementation if allowed) is available, else our decoder only
            UseInternalDecoder = true;
            UseFFmpegDecoder = false;
            UseVulkanDecoder = FFmpeg_Library_Vulkan(AllowVulkanCPU);
        }
        #ifdef RAWCOOKED_FFMPEG
        if (UseFFmpegDecoder || UseVulkanDecoder)
        {
            if (!FFmpeg_Library_Load())
                return nullptr; // Availability of FFmpeg libraries is checked when the decoder is selected
            return new ffv1_hybrid_wrapper(new ffv1_frame_hybrid(Pool, UseInternalDecoder, UseFFmpegDecoder, UseVulkanDecoder)); // FFmpeg decoders, in parallel with our decoder if requested
        }
        #else
        (void)UseInternalDecoder;
        (void)UseFFmpegDecoder;
        (void)UseVulkanDecoder;
        #endif
        return new ffv1_split_wrapper(new ffv1_frame(Pool)); // Our decoder only if no FFmpeg decoder is requested
    CREATEWRAPPER(flac, FLAC);
    CREATEWRAPPER(pcm, PCM);
    case format::None:
    case format::VFW:
        ;
    }
    return nullptr;
}
