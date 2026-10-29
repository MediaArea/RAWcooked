/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FFmpeg_FunctionsH
#define FFmpeg_FunctionsH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFmpeg_Library.h"

//---------------------------------------------------------------------------
// FFmpeg headers are not needed: the parts of the FFmpeg API used here are
// defined below, copied from the FFmpeg 9.0.1 headers (libavcodec 63, libavutil 61),
// structures only up to their last used field (they are allocated by FFmpeg)
// The ABI is stable for a given major version of the libraries, checked when they are loaded
// Define FFMPEG_INCLUDE_REAL for using the FFmpeg headers instead, default if the libraries are linked
//---------------------------------------------------------------------------
#if !defined(FFMPEG_INCLUDE_REAL) && !defined(RAWCOOKED_FFMPEG_DYNAMIC) && !defined(_WIN32) && !defined(__APPLE__)
    #define FFMPEG_INCLUDE_REAL
#endif
#ifdef FFMPEG_INCLUDE_REAL
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
}
#else
#include <cstddef>
#include <cstdint>

// libavutil/version.h, libavcodec/version_major.h
#define LIBAVUTIL_VERSION_MAJOR             61
#define LIBAVCODEC_VERSION_MAJOR            63

// libavutil/macros.h, libavutil/common.h
#define AV_STRINGIFY(s)                     AV_TOSTRING(s)
#define AV_TOSTRING(s)                      #s
#define AV_CEIL_RSHIFT(a, b)                (-((-(a)) >> (b)))
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define AV_NE(be, le)                   (be)
#else
    #define AV_NE(be, le)                   (le)
#endif

// libavutil/log.h
#define AV_LOG_QUIET                        -8

// libavutil/pixfmt.h, libavcodec/codec_id.h, libavutil/hwcontext.h (enums, int sized)
typedef int AVPixelFormat;
#define AV_PIX_FMT_NONE                     -1
#define AV_PIX_FMT_VULKAN                   190
#define AV_CODEC_ID_FFV1                    33
#define AV_HWDEVICE_TYPE_VULKAN             11

// libavcodec/avcodec.h, libavcodec/defs.h
#define FF_THREAD_SLICE                     2
#define AV_INPUT_BUFFER_PADDING_SIZE        64

// libavutil/buffer.h
#define AV_BUFFER_FLAG_READONLY             (1 << 0)

// libavutil/pixdesc.h
#define AV_PIX_FMT_FLAG_BE                  (1 << 0)
#define AV_PIX_FMT_FLAG_PLANAR              (1 << 4)
#define AV_PIX_FMT_FLAG_RGB                 (1 << 5)
#define AV_PIX_FMT_FLAG_ALPHA               (1 << 7)

extern "C"
{

struct AVCodec;
struct AVDictionary;

// libavutil/rational.h
struct AVRational
{
    int                             num;
    int                             den;
};

// libavutil/channel_layout.h
struct AVChannelLayout
{
    int                             order;
    int                             nb_channels;
    union
    {
        uint64_t                    mask;
        void*                       map;
    }                               u;
    void*                           opaque;
};

// libavutil/buffer.h
struct AVBufferRef
{
    void*                           buffer;
    uint8_t*                        data;
    size_t                          size;
};

// libavutil/frame.h
struct AVFrame
{
    uint8_t*                        data[8];
    int                             linesize[8];
    uint8_t**                       extended_data;
    int                             width;
    int                             height;
    int                             nb_samples;
    int                             format;
    int                             pict_type;
    AVRational                      sample_aspect_ratio;
    int64_t                         pts;
    int64_t                         pkt_dts;
    AVRational                      time_base;
    int                             quality;
    void*                           opaque;
    int                             repeat_pict;
    int                             sample_rate;
    AVBufferRef*                    buf[8];
    AVBufferRef**                   extended_buf;
    int                             nb_extended_buf;
    void**                          side_data;
    int                             nb_side_data;
    int                             flags;
    int                             color_range;
    int                             color_primaries;
    int                             color_trc;
    int                             colorspace;
    int                             chroma_location;
    int64_t                         best_effort_timestamp;
    AVDictionary*                   metadata;
    int                             decode_error_flags;
    AVBufferRef*                    hw_frames_ctx;
};

// libavcodec/packet.h
struct AVPacket
{
    AVBufferRef*                    buf;
    int64_t                         pts;
    int64_t                         dts;
    uint8_t*                        data;
    int                             size;
};

// libavcodec/avcodec.h
struct AVCodecContext
{
    const void*                     av_class;
    int                             log_level_offset;
    int                             codec_type;
    const AVCodec*                  codec;
    int                             codec_id;
    unsigned int                    codec_tag;
    void*                           priv_data;
    void*                           internal;
    void*                           opaque;
    int64_t                         bit_rate;
    int                             flags;
    int                             flags2;
    uint8_t*                        extradata;
    int                             extradata_size;
    AVRational                      time_base;
    AVRational                      pkt_timebase;
    AVRational                      framerate;
    int                             delay;
    int                             width;
    int                             height;
    int                             coded_width;
    int                             coded_height;
    AVRational                      sample_aspect_ratio;
    int                             pix_fmt;
    int                             sw_pix_fmt;
    int                             color_primaries;
    int                             color_trc;
    int                             colorspace;
    int                             color_range;
    int                             chroma_sample_location;
    int                             field_order;
    int                             refs;
    int                             has_b_frames;
    int                             slice_flags;
    void                            (*draw_horiz_band)(AVCodecContext* s, const AVFrame* src, int offset[8], int y, int type, int height);
    AVPixelFormat                   (*get_format)(AVCodecContext* s, const AVPixelFormat* fmt);
    int                             max_b_frames;
    float                           b_quant_factor;
    float                           b_quant_offset;
    float                           i_quant_factor;
    float                           i_quant_offset;
    float                           lumi_masking;
    float                           temporal_cplx_masking;
    float                           spatial_cplx_masking;
    float                           p_masking;
    float                           dark_masking;
    int                             nsse_weight;
    int                             me_cmp;
    int                             me_sub_cmp;
    int                             mb_cmp;
    int                             ildct_cmp;
    int                             dia_size;
    int                             last_predictor_count;
    int                             me_pre_cmp;
    int                             pre_dia_size;
    int                             me_subpel_quality;
    int                             me_range;
    int                             mb_decision;
    uint16_t*                       intra_matrix;
    uint16_t*                       inter_matrix;
    uint16_t*                       chroma_intra_matrix;
    int                             intra_dc_precision;
    int                             mb_lmin;
    int                             mb_lmax;
    int                             bidir_refine;
    int                             keyint_min;
    int                             gop_size;
    int                             mv0_threshold;
    int                             slices;
    int                             sample_rate;
    int                             sample_fmt;
    AVChannelLayout                 ch_layout;
    int                             frame_size;
    int                             block_align;
    int                             cutoff;
    int                             audio_service_type;
    int                             request_sample_fmt;
    int                             initial_padding;
    int                             trailing_padding;
    int                             seek_preroll;
    int                             (*get_buffer2)(AVCodecContext* s, AVFrame* frame, int flags);
    int                             bit_rate_tolerance;
    int                             global_quality;
    int                             compression_level;
    float                           qcompress;
    float                           qblur;
    int                             qmin;
    int                             qmax;
    int                             max_qdiff;
    int                             rc_buffer_size;
    int                             rc_override_count;
    void*                           rc_override;
    int64_t                         rc_max_rate;
    int64_t                         rc_min_rate;
    float                           rc_max_available_vbv_use;
    float                           rc_min_vbv_overflow_use;
    int                             rc_initial_buffer_occupancy;
    int                             trellis;
    char*                           stats_out;
    char*                           stats_in;
    int                             workaround_bugs;
    int                             strict_std_compliance;
    int                             error_concealment;
    int                             debug;
    int                             err_recognition;
    const void*                     hwaccel;
    void*                           hwaccel_context;
    AVBufferRef*                    hw_frames_ctx;
    AVBufferRef*                    hw_device_ctx;
    int                             hwaccel_flags;
    int                             extra_hw_frames;
    uint64_t                        error[8];
    int                             dct_algo;
    int                             idct_algo;
    int                             bits_per_coded_sample;
    int                             bits_per_raw_sample;
    int                             thread_count;
    int                             thread_type;
};

// libavutil/pixdesc.h
struct AVComponentDescriptor
{
    int                             plane;
    int                             step;
    int                             offset;
    int                             shift;
    int                             depth;
};
struct AVPixFmtDescriptor
{
    const char*                     name;
    uint8_t                         nb_components;
    uint8_t                         log2_chroma_w;
    uint8_t                         log2_chroma_h;
    uint64_t                        flags;
    AVComponentDescriptor           comp[4];
};

// libavutil/hwcontext.h
struct AVHWDeviceContext
{
    const void*                     av_class;
    int                             type;
    void*                           hwctx;
};
struct AVHWFramesContext
{
    const void*                     av_class;
    AVBufferRef*                    device_ref;
    AVHWDeviceContext*              device_ctx;
    void*                           hwctx;
    void                            (*free)(AVHWFramesContext* ctx);
    void*                           user_opaque;
    void*                           pool;
    int                             initial_pool_size;
    int                             format;
    int                             sw_format;
};

// Functions (libavutil/*.h, libavcodec/*.h)
unsigned                    avutil_version(void);
AVBufferRef*                av_buffer_create(uint8_t* data, size_t size, void (*free)(void* opaque, uint8_t* data), void* opaque, int flags);
AVBufferRef*                av_buffer_ref(const AVBufferRef* buf);
void                        av_buffer_unref(AVBufferRef** buf);
void                        av_dict_free(AVDictionary** m);
int                         av_dict_set(AVDictionary** pm, const char* key, const char* value, int flags);
AVFrame*                    av_frame_alloc(void);
void                        av_frame_free(AVFrame** frame);
void                        av_frame_unref(AVFrame* frame);
void                        av_freep(void* ptr);
int                         av_hwdevice_ctx_create(AVBufferRef** device_ctx, int type, const char* device, AVDictionary* opts, int flags);
int                         av_hwframe_transfer_data(AVFrame* dst, const AVFrame* src, int flags);
void                        av_log_set_level(int level);
void*                       av_mallocz(size_t size);
const AVPixFmtDescriptor*   av_pix_fmt_desc_get(AVPixelFormat pix_fmt);
void                        av_read_image_line2(void* dst, const uint8_t* data[4], const int linesize[4], const AVPixFmtDescriptor* desc, int x, int y, int c, int w, int read_pal_component, int dst_element_size);
unsigned                    avcodec_version(void);
AVPacket*                   av_packet_alloc(void);
void                        av_packet_free(AVPacket** pkt);
void                        av_packet_unref(AVPacket* pkt);
AVCodecContext*             avcodec_alloc_context3(const AVCodec* codec);
const AVCodec*              avcodec_find_decoder(int id);
void                        avcodec_free_context(AVCodecContext** avctx);
int                         avcodec_is_open(AVCodecContext* s);
int                         avcodec_open2(AVCodecContext* avctx, const AVCodec* codec, AVDictionary** options);
int                         avcodec_receive_frame(AVCodecContext* avctx, AVFrame* frame);
int                         avcodec_send_packet(AVCodecContext* avctx, const AVPacket* avpkt);

}
#endif // FFMPEG_INCLUDE_REAL

//***************************************************************************
// Functions of FFmpeg libraries, valid after FFmpeg_Library_Load() returned true
// Called as ffmpeg::Name(), never directly so the libraries can be loaded at run time
//***************************************************************************

// Library, name
#define FFMPEG_FUNCTIONS(_F) \
    _F(avutil,  avutil_version) \
    _F(avutil,  av_buffer_create) \
    _F(avutil,  av_buffer_ref) \
    _F(avutil,  av_buffer_unref) \
    _F(avutil,  av_dict_free) \
    _F(avutil,  av_dict_set) \
    _F(avutil,  av_frame_alloc) \
    _F(avutil,  av_frame_free) \
    _F(avutil,  av_frame_unref) \
    _F(avutil,  av_freep) \
    _F(avutil,  av_hwdevice_ctx_create) \
    _F(avutil,  av_hwframe_transfer_data) \
    _F(avutil,  av_log_set_level) \
    _F(avutil,  av_mallocz) \
    _F(avutil,  av_pix_fmt_desc_get) \
    _F(avutil,  av_read_image_line2) \
    _F(avcodec, avcodec_version) \
    _F(avcodec, av_packet_alloc) \
    _F(avcodec, av_packet_free) \
    _F(avcodec, av_packet_unref) \
    _F(avcodec, avcodec_alloc_context3) \
    _F(avcodec, avcodec_find_decoder) \
    _F(avcodec, avcodec_free_context) \
    _F(avcodec, avcodec_is_open) \
    _F(avcodec, avcodec_open2) \
    _F(avcodec, avcodec_receive_frame) \
    _F(avcodec, avcodec_send_packet) \

namespace ffmpeg
{
    #define FFMPEG_FUNCTION_DECLARE(_LIBRARY, _NAME) extern decltype(&::_NAME) _NAME;
    FFMPEG_FUNCTIONS(FFMPEG_FUNCTION_DECLARE)
    #undef FFMPEG_FUNCTION_DECLARE
}

// Vulkan device with this index in the list of devices (0 for the default device), the default device detected by FFmpeg_Library_Vulkan() is given to the first caller (who owns the reference) so it is not created twice, else a new device (nullptr if not available)
AVBufferRef* FFmpeg_Library_VulkanDevice(size_t Index);

// Vulkan device is a GPU, not a CPU implementation (e.g. lavapipe), SMCount is set to its count of streaming multiprocessors if it is an NVIDIA GPU (else 0), Name to its name
bool FFmpeg_Library_VulkanIsGPU(AVBufferRef* Device, uint32_t* SMCount, std::string* Name = nullptr);

//---------------------------------------------------------------------------
#endif
