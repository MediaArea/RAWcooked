/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Transform/Transform.h"
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Uncompressed/EXR/EXR.h"
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include "Lib/Uncompressed/AVI/AVI.h"
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "Lib/ThirdParty/endianness.h"
//---------------------------------------------------------------------------

//***************************************************************************
// Common
//***************************************************************************

//---------------------------------------------------------------------------
class transform_null : public transform_base
{
public:
    void From(pixel_t*, pixel_t*, pixel_t*, pixel_t*) {}
};

//---------------------------------------------------------------------------
#define JPEG2000RCT(Offset) \
        pixel_t g = y[x]; \
        pixel_t b = u[x]; \
        pixel_t r = v[x]; \
        b -= Offset; \
        r -= Offset; \
        g -= (b + r) >> 2; \
        b += g; \
        r += g; \

//***************************************************************************
// SIMD helpers, selected at run time
//***************************************************************************

// 512-bit vectors are not used: transforms are limited by memory bandwidth, and on some CPUs (e.g. Skylake-SP/X) 512-bit instructions lower
// the frequency of the core, slowing down the decoding done by other threads on the same core (measured: slower than AVX2 and scalar code)
#if defined(_M_X64) || defined(__x86_64__)
#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#define TARGET_AVX2
#else
#define TARGET_AVX2 __attribute__((target("avx2")))
#endif

//---------------------------------------------------------------------------
// 0: none, 1: AVX2
static int Transform_SimdLevel()
{
    static const int Level = []()
    {
        #if defined(_MSC_VER)
            int Info[4];
            __cpuid(Info, 0);
            auto MaxId = Info[0];
            __cpuid(Info, 1);
            if (MaxId < 7 || !(Info[2] & (1 << 27)) || !(Info[2] & (1 << 28))) // OSXSAVE, AVX
                return 0;
            auto XCR0 = _xgetbv(0);
            if ((XCR0 & 0x6) != 0x6) // XMM and YMM states
                return 0;
            __cpuidex(Info, 7, 0);
            if (!(Info[1] & (1 << 5))) // AVX2
                return 0;
            return 1;
        #else
            __builtin_cpu_init();
            return __builtin_cpu_supports("avx2") ? 1 : 0;
        #endif
    }();
    return Level;
}

//---------------------------------------------------------------------------
// Interleaving of R, G, B 16-bit samples, with byte swap if BE
// 16 pixels, 8 pixels per 128-bit lane, 3 output vectors per lane built with byte shuffles of the 3 sources
template<bool BE>
struct interleave_16_rgb_avx2
{
    alignas(32) uint8_t         Masks[3][3][32]; // Output, source

    interleave_16_rgb_avx2()
    {
        for (size_t o = 0; o < 3; o++)
            for (size_t c = 0; c < 3; c++)
                for (size_t j = 0; j < 8; j++)
                {
                    auto n = o * 8 + j; // Output sample index in the lane
                    auto p = n / 3; // Pixel
                    auto Match = n % 3 == c;
                    for (size_t Lane = 0; Lane < 2; Lane++)
                    {
                        Masks[o][c][Lane * 16 + j * 2    ] = Match ? (uint8_t)(p * 2 + (BE ? 1 : 0)) : 0x80;
                        Masks[o][c][Lane * 16 + j * 2 + 1] = Match ? (uint8_t)(p * 2 + (BE ? 0 : 1)) : 0x80;
                    }
                }
    }

    static const interleave_16_rgb_avx2& Get()
    {
        static const interleave_16_rgb_avx2 Value;
        return Value;
    }
};
#define INTERLEAVE_16_RGB_AVX2_INIT(BE) \
    __m256i M[3][3]; \
    { \
        const auto& Interleave = interleave_16_rgb_avx2<BE>::Get(); \
        for (size_t o = 0; o < 3; o++) \
            for (size_t c = 0; c < 3; c++) \
                M[o][c] = _mm256_load_si256((const __m256i*)Interleave.Masks[o][c]); \
    }
TARGET_AVX2 static inline void Interleave_16_RGB_AVX2(uint8_t* Dest, const __m256i* S, const __m256i M[3][3])
{
    __m256i O[3];
    for (size_t o = 0; o < 3; o++)
        O[o] = _mm256_or_si256(_mm256_or_si256(_mm256_shuffle_epi8(S[0], M[o][0]), _mm256_shuffle_epi8(S[1], M[o][1])), _mm256_shuffle_epi8(S[2], M[o][2]));
    auto D = (__m256i*)Dest;
    _mm256_storeu_si256(D    , _mm256_permute2x128_si256(O[0], O[1], 0x20)); // Pixels 0-7: O0, O1, O2 of 1st lane
    _mm256_storeu_si256(D + 1, _mm256_permute2x128_si256(O[2], O[0], 0x30)); // Then pixels 8-15: O0, O1, O2 of 2nd lane
    _mm256_storeu_si256(D + 2, _mm256_permute2x128_si256(O[1], O[2], 0x31));
}

//---------------------------------------------------------------------------
// JPEG2000-RCT to RGB, 16-bit containers, returns the count of pixels done (the caller does the remaining pixels)
template<bool BE>
TARGET_AVX2 static size_t RCT_16_RGB_AVX2(uint8_t* Dest, const pixel_t* y, const pixel_t* u, const pixel_t* v, size_t w)
{
    INTERLEAVE_16_RGB_AVX2_INIT(BE);
    const auto Offset = _mm256_set1_epi32(1 << 16);
    const auto Mask = _mm256_set1_epi32(0xFFFF); // Same as the truncation in scalar code
    size_t x = 0;
    for (; x + 16 <= w; x += 16)
    {
        __m256i C[3][2]; // R, G, B, 32-bit
        for (size_t h = 0; h < 2; h++)
        {
            auto Y = _mm256_loadu_si256((const __m256i*)(y + x + h * 8));
            auto B = _mm256_sub_epi32(_mm256_loadu_si256((const __m256i*)(u + x + h * 8)), Offset);
            auto R = _mm256_sub_epi32(_mm256_loadu_si256((const __m256i*)(v + x + h * 8)), Offset);
            auto G = _mm256_sub_epi32(Y, _mm256_srai_epi32(_mm256_add_epi32(B, R), 2));
            C[0][h] = _mm256_and_si256(_mm256_add_epi32(R, G), Mask);
            C[1][h] = _mm256_and_si256(G, Mask);
            C[2][h] = _mm256_and_si256(_mm256_add_epi32(B, G), Mask);
        }
        __m256i S[3];
        for (size_t c = 0; c < 3; c++)
            S[c] = _mm256_permute4x64_epi64(_mm256_packus_epi32(C[c][0], C[c][1]), 0xD8); // Packing is per 128-bit lane
        Interleave_16_RGB_AVX2(Dest + x * 6, S, M);
    }
    return x;
}
//---------------------------------------------------------------------------
// JPEG2000-RCT to RGB, 10-bit in 32-bit containers, method A, with g and b inverted (exception in specs), returns the count of pixels done
template<bool BE>
TARGET_AVX2 static size_t RCT_10_FilledA_RGB_AVX2(uint8_t* Dest, const pixel_t* y, const pixel_t* u, const pixel_t* v, size_t w)
{
    const auto Offset = _mm256_set1_epi32(1 << 10);
    const auto Swap = _mm256_set_epi8(
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3,
        12, 13, 14, 15, 8, 9, 10, 11, 4, 5, 6, 7, 0, 1, 2, 3);
    size_t x = 0;
    for (; x + 8 <= w; x += 8)
    {
        auto Y = _mm256_loadu_si256((const __m256i*)(y + x));
        auto B = _mm256_sub_epi32(_mm256_loadu_si256((const __m256i*)(u + x)), Offset);
        auto R = _mm256_sub_epi32(_mm256_loadu_si256((const __m256i*)(v + x)), Offset);
        auto G = _mm256_sub_epi32(Y, _mm256_srai_epi32(_mm256_add_epi32(B, R), 2));
        B = _mm256_add_epi32(B, G);
        R = _mm256_add_epi32(R, G);
        auto O = _mm256_or_si256(_mm256_or_si256(_mm256_slli_epi32(R, 22), _mm256_slli_epi32(B, 12)), _mm256_slli_epi32(G, 2));
        if (BE)
            O = _mm256_shuffle_epi8(O, Swap);
        _mm256_storeu_si256((__m256i*)(Dest + x * 4), O);
    }
    return x;
}
// SIMD version if available, x is the count of pixels done
#define TRANSFORM_SIMD_CALL(NAME, BE, Dest) \
    if (Transform_SimdLevel()) \
        x = NAME##_AVX2<BE>(Dest, y, u, v, w);
#else
#define TRANSFORM_SIMD_CALL(NAME, BE, Dest)
#endif

//***************************************************************************
// DPX
//***************************************************************************

//---------------------------------------------------------------------------
class transform_dpx : public transform_base
{
public:
    transform_dpx(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t) :
        w(w)
    {
        const auto& Plane = RawFrame->Plane(0);
        FrameBuffer = Plane->Buffer().Data()
            + y_offset * (Plane->BitsPerLine() / 8)
            + x_offset * (Plane->BitsPerBlock() / 8) / Plane->PixelsPerBlock(); //TODO: check when not byte boundary

        NextLine_Offset = (Plane->BitsPerLine() / 8) - w * (Plane->BitsPerBlock() / 8) / Plane->PixelsPerBlock();
    }

    inline void Next()
    {
        FrameBuffer += NextLine_Offset;
    }

protected:
    uint8_t*    FrameBuffer;
    size_t      w;
    size_t      NextLine_Offset;
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_8 : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_8(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 8);
            *(FrameBuffer++) = (uint8_t)r;
            *(FrameBuffer++) = (uint8_t)g;
            *(FrameBuffer++) = (uint8_t)b;
        }

        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_10_FilledA_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_10_FilledA_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        size_t x = 0;
        TRANSFORM_SIMD_CALL(RCT_10_FilledA_RGB, false, FrameBuffer);
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer + x;

        for (; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 10);
            *(FrameBuffer_Temp_32++) = htol((uint32_t)((r << 22) | (b << 12) | (g << 2))); // Exception indicated in specs, g and b are inverted
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_10_FilledA_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_10_FilledA_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        size_t x = 0;
        TRANSFORM_SIMD_CALL(RCT_10_FilledA_RGB, true, FrameBuffer);
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer + x;

        for (; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 10);
            *(FrameBuffer_Temp_32++) = htob((uint32_t)((r << 22) | (b << 12) | (g << 2))); // Exception indicated in specs, g and b are inverted
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
void transform_jpeg2000rct_dpx_Raw_RGB_12_Packed_BE_Finalize(raw_frame* RawFrame, size_t num_h_slices, size_t /*num_v_slices*/)
{
    const auto& Plane = RawFrame->Plane(0);
    auto FrameBufferBefore = Plane->Buffer_Extra().Data();
    auto Line_SliceCount = Plane->Line_SliceCount();

    const auto Width = Plane->Width();
    const auto Height = Plane->Height();
    size_t Slices_w = Width / num_h_slices;
    for (size_t y = 0; y < Height; y++)
    {
        for (size_t slice_x = 0; slice_x < num_h_slices - 1; slice_x++)
        {
            auto x = slice_x * Width / num_h_slices;
            auto x2 = (slice_x + 1) * Width / num_h_slices;
            if (!(x2 % 8))
                continue;
            auto FrameBuffer = Plane->Buffer_Data(x2, y, true);
            auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;
            auto FrameBufferBefore_Temp_32 = (uint32_t*)FrameBufferBefore + x / Slices_w;
            *FrameBuffer_Temp_32 |= *FrameBufferBefore_Temp_32;
        }
        FrameBufferBefore += Line_SliceCount * 4;
    }
}

class transform_jpeg2000rct_dpx_Raw_RGB_12_Packed_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_12_Packed_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h)
    {
        const auto& Plane = RawFrame->Plane(0);
        auto Width = Plane->Width();
        auto PixelsPerBlock = dpx::PixelsPerBlock((dpx::flavor)RawFrame->Flavor_Private);
        auto HasBlockSpan = (x_offset % PixelsPerBlock) || ((x_offset + w) % PixelsPerBlock);
        bool VFlip = dpx::IsVFlip(RawFrame->Flavor_Private);

        if (!HasBlockSpan && !VFlip)
        {
            FrameBufferBefore = nullptr;
            Data_Temp_Pos_Begin = 0;
            return;
        }

        if (HasBlockSpan && !x_offset && !y_offset)
            RawFrame->Finalize_Function = transform_jpeg2000rct_dpx_Raw_RGB_12_Packed_BE_Finalize;

        if (VFlip)
        {
            auto Height = Plane->Height();
            y_offset = Height - 1 - y_offset;
        }

        auto FullLineBitCount = Width * 36;
        auto FullLineBlockCount = FullLineBitCount / 32;
        if (FullLineBitCount % 32)
            FullLineBlockCount++;
        auto SliceBeginBitCount = x_offset * 36;
        auto SliceBeginBlockCount = SliceBeginBitCount / 32;
        auto SliceEndBitCount = (x_offset + w) * 36;
        auto SliceEndBlockCount = SliceEndBitCount / 32;

        FrameBuffer = Plane->Buffer_Data() + (y_offset * FullLineBlockCount + SliceBeginBlockCount) * 4;
        NextLine_Offset = SliceBeginBlockCount - SliceEndBlockCount; // -(SliceEndBlockCount - SliceBeginBlockCount)
        if (VFlip)
            NextLine_Offset -= FullLineBlockCount;
        else
            NextLine_Offset += FullLineBlockCount;
        NextLine_Offset *= 4;
        Data_Temp_Pos_Begin = x_offset % 8;
        
        if (HasBlockSpan && x_offset + w < Width)
        {
            auto Line_SliceCount = Plane->Line_SliceCount();
            FrameBufferBefore = Plane->Buffer_Extra().Data() + (Line_SliceCount * y_offset + (x_offset + 1) * Line_SliceCount / Width) * 4;
            FrameBufferBefore_Offset = (uint32_t)(Line_SliceCount * 4);
        }
        else
            FrameBufferBefore = nullptr; // Last block in a line is padded so should be written immediately
    }

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        uint32_t Data_Temp = 0;
        auto Data_Temp_Pos = Data_Temp_Pos_Begin;
        uint32_t* FrameBuffer_Temp_32;
        size_t x = 0;

        FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (; x < w; x++)
        {
            switch (Data_Temp_Pos)
            {
            case 0:
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((b << 24) | (g << 12) | r));
                Data_Temp = (b >> 8);
                break;
            }
            case 1:
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((b << 28) | (g << 16) | (r << 4) | Data_Temp));
                Data_Temp = (b >> 4);
                break;
            }
            case 2:
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((g << 20) | (r << 8) | Data_Temp));
                Data_Temp = b;
                break;
            }
            case 3:
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((g << 24) | (r << 12) | Data_Temp));
                Data_Temp = (b << 4) | (g >> 8);
                break;
            }
            case 4:
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((g << 28) | (r << 16) | Data_Temp));
                Data_Temp = (b << 8) | (g >> 4);
                break;
            }
            case 5:
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((r << 20) | Data_Temp));
                Data_Temp = (b << 12) | g;
                break;
            }
            case 6:
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((r << 24) | Data_Temp));
                Data_Temp = (b << 16) | (g << 4) | (r >> 8);
                break;
            }
            default:
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                swap(b, g); // Exception indicated in specs, g and b are inverted
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((r << 28) | Data_Temp));
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((b << 20) | (g << 8) | (r >> 4)));
                break;
            }
            }
            Data_Temp_Pos = (Data_Temp_Pos + 1 ) % 8;
        }
        if (Data_Temp_Pos)
        {
            if (FrameBufferBefore)
            {
                auto FrameBufferBefore_Temp_32 = (uint32_t*)FrameBufferBefore;
                *FrameBufferBefore_Temp_32 = htob(Data_Temp);
                if ((intptr_t)NextLine_Offset < 0)
                    FrameBufferBefore -= FrameBufferBefore_Offset;
                else
                    FrameBufferBefore += FrameBufferBefore_Offset;
            }
            else
            {
                *FrameBuffer_Temp_32 = htob(Data_Temp);
            }
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }

protected:
    uint8_t*    FrameBufferBefore;
    uint32_t    Data_Temp_Pos_Begin;
    uint32_t    FrameBufferBefore_Offset;
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_12_FilledA_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_12_FilledA_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 12);
            swap(b, g); // Exception indicated in specs, g and b are inverted
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(r << 4));
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(g << 4));
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(b << 4));
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_12_FilledA_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_12_FilledA_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 12);
            swap(b, g); // Exception indicated in specs, g and b are inverted
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(r << 4));
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(g << 4));
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(b << 4));
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_16_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_16_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        size_t x = 0;
        TRANSFORM_SIMD_CALL(RCT_16_RGB, false, FrameBuffer);
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer + x * 3;

        for (; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 16);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)r);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)g);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)b);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGB_16_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGB_16_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        size_t x = 0;
        TRANSFORM_SIMD_CALL(RCT_16_RGB, true, FrameBuffer);
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer + x * 3;

        for (; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 16);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)r);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)g);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)b);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_8 : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_8(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 8);
            *(FrameBuffer++) = (uint8_t)r;
            *(FrameBuffer++) = (uint8_t)g;
            *(FrameBuffer++) = (uint8_t)b;
            *(FrameBuffer++) = (uint8_t)a[x];
        }

        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_10_FilledA_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_10_FilledA_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            uint32_t Data_Temp;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htol((uint32_t)((r << 22) | (g << 12) | (b << 2)));
                Data_Temp = (a[x] << 22);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htol((uint32_t)(Data_Temp | (r << 12) | (g << 2)));
                Data_Temp = (b << 22) | (a[x] << 12);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htol((uint32_t)(Data_Temp | (r << 2)));
                *(FrameBuffer_Temp_32++) = htol((uint32_t)((g << 22) | (b << 12) | (a[x] << 2)));
            }
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_10_FilledA_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_10_FilledA_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            uint32_t Data_Temp;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((r << 22) | (g << 12) | (b << 2)));
                Data_Temp = (a[x] << 22);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htob((uint32_t)(Data_Temp | (r << 12) | (g << 2)));
                Data_Temp = (b << 22) | (a[x] << 12);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 10);
                *(FrameBuffer_Temp_32++) = htob((uint32_t)(Data_Temp | (r << 2)));
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((g << 22) | (b << 12) | (a[x] << 2)));
            }
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_12_Packed_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_12_Packed_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            uint32_t Data_Temp;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((b << 24) | (g << 12) | r));
                Data_Temp = (a[x] << 4) | (b >> 8);
            }
            x++;
            {
                JPEG2000RCT(((pixel_t)1) << 12);
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((g << 28) | (r << 16) | (Data_Temp)));
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((a[x] << 20) | (b << 8) | (g >> 4)));
            }
        }
    
        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_12_FilledA_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_12_FilledA_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 12);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(r << 4));
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(g << 4));
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(b << 4));
            *(FrameBuffer_Temp_16++) = htol((uint16_t)(a[x] << 4));
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_12_FilledA_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_12_FilledA_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 12);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(r << 4));
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(g << 4));
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(b << 4));
            *(FrameBuffer_Temp_16++) = htob((uint16_t)(a[x] << 4));
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_16_LE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_16_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 16);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)r);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)g);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)b);
            *(FrameBuffer_Temp_16++) = htol((uint16_t)a[x]);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_dpx_Raw_RGBA_16_BE : public transform_dpx
{
public:
    transform_jpeg2000rct_dpx_Raw_RGBA_16_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t* a)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 16);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)r);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)g);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)b);
            *(FrameBuffer_Temp_16++) = htob((uint16_t)a[x]);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_passthrough_dpx_Raw_Y_8 : public transform_dpx
{
public:
    transform_passthrough_dpx_Raw_Y_8(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t*, pixel_t*, pixel_t*)
    {
        for (size_t x = 0; x < w; x++)
        {
            *(FrameBuffer++) = (int8_t)y[x];
        }

        Next();
    }
};

//---------------------------------------------------------------------------
void transform_passthrough_dpx_Raw_Y_10_FilledX_BE_Finalize(raw_frame* RawFrame, size_t num_h_slices, size_t /*num_v_slices*/)
{
    const auto& Plane = RawFrame->Plane(0);
    auto FrameBufferBefore = Plane->Buffer_Extra().Data();
    if (!FrameBufferBefore)
        return;

    const auto Width = Plane->Width();
    const auto Height = Plane->Height();
    const auto Line_SliceCount = Plane->Line_SliceCount();
    auto PixelCount = Width * Height;
    auto Altern = dpx::IsAltern(RawFrame->Flavor_Private);
    if (Altern && PixelCount % 3)
    {
        // Last frame block is never written by the transform, setting to 0
        auto FrameBuffer = Plane->Buffer().Data() + Plane->Buffer().Size() - 4;
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;
        *FrameBuffer_Temp_32 = 0;
    }

    size_t Slices_w = Width / num_h_slices;
    for (size_t y = 0; y < Height; y++)
    {
        for (size_t slice_x = 0; slice_x < num_h_slices - (Altern ? 0 : 1); slice_x++)
        {
            auto x = slice_x * Width / num_h_slices;
            auto x2 = (slice_x + 1) * Width / num_h_slices;
            if (Altern && !((y * Width + x2) % 3))
                continue;
            auto FrameBuffer = Plane->Buffer_Data(x2, y, true);
            auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;
            auto FrameBufferBefore_Temp_32 = (uint32_t*)FrameBufferBefore + x / Slices_w;
            *FrameBuffer_Temp_32 |= *FrameBufferBefore_Temp_32;
        }
        FrameBufferBefore += Line_SliceCount * 4;
    }
}

template <int Offset> class transform_passthrough_dpx_Raw_Y_10_FilledX_BE : public transform_dpx
{
public:
    transform_passthrough_dpx_Raw_Y_10_FilledX_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h)
    {
        const auto& Plane = RawFrame->Plane(0);
        Width = Plane->Width();
        auto PixelsPerBlock = dpx::PixelsPerBlock((dpx::flavor)RawFrame->Flavor_Private);
        Altern = dpx::IsAltern(RawFrame->Flavor_Private);
        bool HasBlockSpan;
        if (Altern)
            HasBlockSpan = true;
        else
            HasBlockSpan = (x_offset % PixelsPerBlock) || ((x_offset + w) % PixelsPerBlock);

        if (!HasBlockSpan)
        {
            FrameBufferBefore = nullptr;
            Data_Temp_Pos_Begin = 0;
            return;
        }

        if (!x_offset && !y_offset)
            RawFrame->Finalize_Function = transform_passthrough_dpx_Raw_Y_10_FilledX_BE_Finalize;

        if (Altern)
        {
            auto PixelCountBefore = y_offset * Width + x_offset;
            auto SliceBeginBlockCount = PixelCountBefore / 3;

            FrameBuffer = Plane->Buffer_Data() + SliceBeginBlockCount * 4;
            Data_Temp_Pos_Begin = PixelCountBefore % 3;
        }
        else
        {
            auto FullLineBlockCount = Width / 3;
            if (Width % 3)
                FullLineBlockCount++;
            auto SliceBeginBlockCount = x_offset / 3;
            auto SliceEndBlockCount = (x_offset + w) / 3;

            NextLine_Offset = SliceBeginBlockCount - SliceEndBlockCount; // -(SliceEndBlockCount - SliceBeginBlockCount)
            NextLine_Offset += FullLineBlockCount;
            NextLine_Offset *= 4;

            FrameBuffer = Plane->Buffer_Data(x_offset, y_offset);
            Data_Temp_Pos_Begin = x_offset % 3;
        }

        if ((Altern || x_offset + w < Width))
        {
            auto Line_SliceCount = Plane->Line_SliceCount();
            FrameBufferBefore = Plane->Buffer_Extra().Data() + (Line_SliceCount * y_offset + (x_offset + 1) * Line_SliceCount / Width) * 4;
            FrameBufferBefore_Offset = (uint32_t)(Line_SliceCount * 4);
        }
        else
            FrameBufferBefore = nullptr; // Last block in a line is padded so should be written immediately
    };

    void From(pixel_t* y, pixel_t*, pixel_t*, pixel_t*)
    {
        uint32_t Data_Temp = 0;
        auto Data_Temp_Pos = Data_Temp_Pos_Begin;
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            Data_Temp |= ((uint32_t)y[x]) << (Data_Temp_Pos * 10 + Offset);
            if (Data_Temp_Pos == 2)
            {
                *(FrameBuffer_Temp_32++) = htob(Data_Temp);
                Data_Temp = 0;
                Data_Temp_Pos = 0;
                continue;
            }
            Data_Temp_Pos++;
        }
        if (FrameBufferBefore)
        {
            auto FrameBufferBefore_Temp_32 = (uint32_t*)FrameBufferBefore;
            *FrameBufferBefore_Temp_32 = htob(Data_Temp);
            if ((intptr_t)NextLine_Offset < 0)
                FrameBufferBefore -= FrameBufferBefore_Offset;
            else
                FrameBufferBefore += FrameBufferBefore_Offset;
        }
        else if (!Altern && Data_Temp_Pos)
        {
            *FrameBuffer_Temp_32 = htob(Data_Temp);
        }
        if (Altern)
        {
            auto NextPixelCountBefore = Data_Temp_Pos + Width - w;
            auto NextBlockOffset = NextPixelCountBefore / 3;
            Data_Temp_Pos_Begin = NextPixelCountBefore % 3;
            FrameBuffer_Temp_32 += NextBlockOffset;
        }
        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        if (!Altern)
            Next();
     }

protected:
    uint8_t*    FrameBufferBefore;
    uint32_t    Data_Temp_Pos_Begin;
    uint32_t    FrameBufferBefore_Offset;
    size_t      Width;
    bool        Altern;
};

#define transform_passthrough_dpx_Raw_Y_10_FilledA_BE transform_passthrough_dpx_Raw_Y_10_FilledX_BE<2>
#define transform_passthrough_dpx_Raw_Y_10_FilledB_BE transform_passthrough_dpx_Raw_Y_10_FilledX_BE<0>

//---------------------------------------------------------------------------
void transform_passthrough_dpx_Raw_Y_12_Packed_BE_Finalize(raw_frame* RawFrame, size_t num_h_slices, size_t /*num_v_slices*/)
{
    const auto& Plane = RawFrame->Plane(0);
    auto FrameBufferBefore = Plane->Buffer_Extra().Data();
    auto Line_SliceCount = Plane->Line_SliceCount();

    const auto Width = Plane->Width();
    const auto Height = Plane->Height();
    size_t Slices_w = Width / num_h_slices;
    for (size_t y = 0; y < Height; y++)
    {
        for (size_t slice_x = 0; slice_x < num_h_slices - 1; slice_x++)
        {
            auto x = slice_x * Width / num_h_slices;
            auto x2 = (slice_x + 1) * Width / num_h_slices;
            if (!(x2 % 8))
                continue;
            auto FrameBuffer = Plane->Buffer_Data(x2, y, true);
            auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;
            auto FrameBufferBefore_Temp_32 = (uint32_t*)FrameBufferBefore + x / Slices_w;
            *FrameBuffer_Temp_32 |= *FrameBufferBefore_Temp_32;
            FrameBuffer_Temp_32++;
        }
        FrameBufferBefore += Line_SliceCount * 4;
    }
}

class transform_passthrough_dpx_Raw_Y_12_Packed_BE : public transform_dpx
{
public:
    transform_passthrough_dpx_Raw_Y_12_Packed_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h)
    {
        const auto& Plane = RawFrame->Plane(0);
        auto Width = Plane->Width();
        auto PixelsPerBlock = dpx::PixelsPerBlock((dpx::flavor)RawFrame->Flavor_Private);
        auto HasBlockSpan = (x_offset % PixelsPerBlock) || ((x_offset + w) % PixelsPerBlock);
        bool VFlip = dpx::IsVFlip(RawFrame->Flavor_Private);

        if (!HasBlockSpan && !VFlip)
        {
            FrameBufferBefore = nullptr;
            Data_Temp_Pos_Begin = 0;
            return;
        }

        if (HasBlockSpan && !x_offset && !y_offset)
            RawFrame->Finalize_Function = transform_passthrough_dpx_Raw_Y_12_Packed_BE_Finalize;

        if (VFlip)
        {
            auto Height = Plane->Height();
            y_offset = Height - 1 - y_offset;
        }

        auto FullLineBitCount = Width * 12;
        auto FullLineBlockCount = FullLineBitCount / 32;
        if (FullLineBitCount % 32)
            FullLineBlockCount++;
        auto SliceBeginBitCount = x_offset * 12;
        auto SliceBeginBlockCount = SliceBeginBitCount / 32;
        auto SliceEndBitCount = (x_offset + w) * 12;
        auto SliceEndBlockCount = SliceEndBitCount / 32;

        FrameBuffer = Plane->Buffer_Data() + (y_offset * FullLineBlockCount + SliceBeginBlockCount) * 4;
        NextLine_Offset = SliceBeginBlockCount - SliceEndBlockCount; // -(SliceEndBlockCount - SliceBeginBlockCount)
        if (VFlip)
            NextLine_Offset -= FullLineBlockCount;
        else
            NextLine_Offset += FullLineBlockCount;
        NextLine_Offset *= 4;
        Data_Temp_Pos_Begin = x_offset % 8;
        
        if (HasBlockSpan && x_offset + w < Width)
        {
            auto Line_SliceCount = Plane->Line_SliceCount();
            FrameBufferBefore = Plane->Buffer_Extra().Data() + (Line_SliceCount * y_offset + (x_offset + 1) * Line_SliceCount / Width) * 4;
            FrameBufferBefore_Offset = (uint32_t)(Line_SliceCount * 4);
        }
        else
            FrameBufferBefore = nullptr; // Last block in a line is padded so should be written immediately
    }

    void From(pixel_t* y, pixel_t*, pixel_t*, pixel_t*)
    {
        uint32_t Data_Temp = 0;
        auto Data_Temp_Pos = Data_Temp_Pos_Begin;
        uint32_t* FrameBuffer_Temp_32;
        size_t x = 0;

        FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        for (; x < w; x++)
        {
            switch (Data_Temp_Pos)
            {
            case 0:
            {
                Data_Temp  = y[x];
                break;
            }
            case 1:
            {
                Data_Temp |= y[x] << 12;
                break;
            }
            case 2:
            {
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((y[x] << 24) | Data_Temp));
                Data_Temp  = y[x] >> 8;
                break;
            }
            case 3:
            {
                Data_Temp |= y[x] << 4;
                break;
            }
            case 4:
            {
                Data_Temp |= y[x] << 16;
                break;
            }
            case 5:
            {
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((y[x] << 28) | Data_Temp));
                Data_Temp  = y[x] >>  4;
                break;
            }
            case 6:
            {
                Data_Temp |= y[x] <<  8;
                break;
            }
            default:
            {
                *(FrameBuffer_Temp_32++) = htob((uint32_t)((y[x] << 20) | Data_Temp));
                break;
            }
            }
            Data_Temp_Pos = (Data_Temp_Pos + 1 ) % 8;
        }
        if (Data_Temp_Pos)
        {
            if (FrameBufferBefore)
            {
                auto FrameBufferBefore_Temp_32 = (uint32_t*)FrameBufferBefore;
                *FrameBufferBefore_Temp_32 = htob(Data_Temp);
                if ((intptr_t)NextLine_Offset < 0)
                    FrameBufferBefore -= FrameBufferBefore_Offset;
                else
                    FrameBufferBefore += FrameBufferBefore_Offset;
            }
            else
            {
                *FrameBuffer_Temp_32 = htob(Data_Temp);
            }
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_32;
        Next();
    }

protected:
    uint8_t*    FrameBufferBefore;
    uint32_t    Data_Temp_Pos_Begin;
    uint32_t    FrameBufferBefore_Offset;
};

//---------------------------------------------------------------------------
class transform_passthrough_dpx_Raw_Y_16_LE : public transform_dpx
{
public:
    transform_passthrough_dpx_Raw_Y_16_LE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t*, pixel_t*, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            *(FrameBuffer_Temp_16++) = htol((uint16_t)y[x]);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//---------------------------------------------------------------------------
class transform_passthrough_dpx_Raw_Y_16_BE : public transform_dpx
{
public:
    transform_passthrough_dpx_Raw_Y_16_BE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_dpx(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t*, pixel_t*, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            *(FrameBuffer_Temp_16++) = htob((uint16_t)y[x]);
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//***************************************************************************
// TIFF
//***************************************************************************

//---------------------------------------------------------------------------
#define SAMEAS(PIX, SOURCE, DEST) \
class transform_##PIX##_##SOURCE : public transform_##PIX##_##DEST \
{ \
public: \
    transform_##PIX##_##SOURCE(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) : \
        transform_##PIX##_##DEST(RawFrame, x_offset, y_offset, w, h) {}; \
}; \

//---------------------------------------------------------------------------
SAMEAS(jpeg2000rct, tiff_Raw_RGB_8_U       , dpx_Raw_RGB_8)
SAMEAS(jpeg2000rct, tiff_Raw_RGB_16_U_LE   , dpx_Raw_RGB_16_LE)
SAMEAS(jpeg2000rct, tiff_Raw_RGB_16_U_BE   , dpx_Raw_RGB_16_BE)
SAMEAS(jpeg2000rct, tiff_Raw_RGBA_8_U      , dpx_Raw_RGBA_8)
SAMEAS(jpeg2000rct, tiff_Raw_RGBA_16_U_LE  , dpx_Raw_RGBA_16_LE)
SAMEAS(passthrough, tiff_Raw_Y_8_U         , dpx_Raw_Y_8)
SAMEAS(passthrough, tiff_Raw_Y_16_U_LE     , dpx_Raw_Y_16_LE)
SAMEAS(passthrough, tiff_Raw_Y_16_U_BE     , dpx_Raw_Y_16_BE)

//***************************************************************************
// EXR
//***************************************************************************

//---------------------------------------------------------------------------
class transform_exr : public transform_base
{
public:
    static const size_t Plane_Count = 3; // TODO: handle when not 3 components
    transform_exr(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        w(w)
    {
        const auto& Plane = RawFrame->Plane(0);
        FrameBuffer = Plane->Buffer_Data(0, y_offset); // x_offset span is not supported
        if (x_offset)
        {
            FrameBuffer += x_offset * (Plane->BitsPerBlock() / 8) / Plane->PixelsPerBlock() / Plane_Count;
        }
        else
        {
            auto FrameBuffer_Temp2 = FrameBuffer;
            for (size_t y = 0; y < h; ++y)
            {
                auto* FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer_Temp2;
                FrameBuffer_Temp_32[0] = htol((uint32_t)(y_offset + y));
                FrameBuffer_Temp_32[1] = htol((uint32_t)(Plane->ValidBitsPerLine() / 8));
                FrameBuffer_Temp2 += (Plane->BitsPerLine() / 8);
            }
        }

        FrameBuffer += 8;
        FrameWidth = Plane->Width();
        NextLine_Offset = (Plane->BitsPerLine() / 8) - w * (Plane->BitsPerBlock() / 8) / Plane->PixelsPerBlock() / Plane_Count;
    }

    inline void Next()
    {
        FrameBuffer += NextLine_Offset;
    }

protected:
    uint8_t*    FrameBuffer;
    size_t      w;
    size_t      FrameWidth;
    size_t      NextLine_Offset;
};

//---------------------------------------------------------------------------
class transform_jpeg2000rct_exr_Raw_RGB_16 : public transform_exr
{
public:
    transform_jpeg2000rct_exr_Raw_RGB_16(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_exr(RawFrame, x_offset, y_offset, w, h) {};

    void From(pixel_t* y, pixel_t* u, pixel_t* v, pixel_t*)
    {
        auto FrameBuffer_Temp_16 = (uint16_t*)FrameBuffer;

        for (size_t x = 0; x < w; x++)
        {
            JPEG2000RCT(((pixel_t)1) << 16);
            FrameBuffer_Temp_16[0] = htol((uint16_t)b);
            FrameBuffer_Temp_16[FrameWidth] = htol((uint16_t)g);
            FrameBuffer_Temp_16[FrameWidth * 2] = htol((uint16_t)r);
            FrameBuffer_Temp_16++;
        }

        FrameBuffer = (uint8_t*)FrameBuffer_Temp_16;
        Next();
    }
};

//***************************************************************************
// v210
//***************************************************************************

//---------------------------------------------------------------------------
class transform_v210 : public transform_base
{
public:
    transform_v210(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        w(w),
        h(h)
    {
        const auto& Plane = RawFrame->Plane(0);
        FrameBuffer = Plane->Buffer().Data()
            + y_offset * (Plane->BitsPerLine() / 8)
            + x_offset * (Plane->BitsPerBlock() / 8) / Plane->PixelsPerBlock(); //TODO: check when not byte boundary

        NextLine_Offset = Plane->Width() * 8 / 3;
        Line = h;
        Pass = 0;
    }

    inline void Next()
    {
        FrameBuffer += NextLine_Offset;
    }

    void From(pixel_t* y, pixel_t*, pixel_t*, pixel_t*)
    {
        auto FrameBuffer_Temp_32 = (uint32_t*)FrameBuffer;

        switch (Pass)
        {
        case 0:
            for (size_t x = 0; x < w; x += 3)
            {
                *(FrameBuffer_Temp_32++) = htol(((uint32_t)y[x    ] << 10)                             );
                *(FrameBuffer_Temp_32++) = htol(((uint32_t)y[x + 1]      ) | ((uint32_t)y[x + 2] << 20));
            }
            break;
        case 1:
           for (size_t x = 0; x < w; x += 3)
           {
               *FrameBuffer_Temp_32 = htol(ltoh(*FrameBuffer_Temp_32) | ((uint32_t)y[x    ]      ));
               FrameBuffer_Temp_32++;
               *FrameBuffer_Temp_32 = htol(ltoh(*FrameBuffer_Temp_32) | ((uint32_t)y[x + 1] << 10));
               FrameBuffer_Temp_32++;
               *FrameBuffer_Temp_32 = htol(ltoh(*FrameBuffer_Temp_32) | ((uint32_t)y[x + 2] << 20));
               FrameBuffer_Temp_32++;
               FrameBuffer_Temp_32++;
           }
            break;
        case 2:
           for (size_t x = 0; x < w; x += 3)
           {
               *FrameBuffer_Temp_32 = htol(ltoh(*FrameBuffer_Temp_32) | ((uint32_t)y[x    ] << 20));
               FrameBuffer_Temp_32++;
               FrameBuffer_Temp_32++;
               *FrameBuffer_Temp_32 = htol(ltoh(*FrameBuffer_Temp_32) | ((uint32_t)y[x + 1]      ));
               FrameBuffer_Temp_32++;
               *FrameBuffer_Temp_32 = htol(ltoh(*FrameBuffer_Temp_32) | ((uint32_t)y[x + 2] << 10));
               FrameBuffer_Temp_32++;
           }
            break;
        }

        Next();
        Line--;
        if (!Line)
        {
            FrameBuffer -= h * NextLine_Offset;
            Line = h;
            if (!Pass)
                w /= 2;
            Pass++;
        }
    }

protected:
    uint8_t*    FrameBuffer;
    size_t      w;
    size_t      h;
    size_t      NextLine_Offset;
    size_t      Line;
    size_t      Pass;
};

//***************************************************************************
// AVI
//***************************************************************************

//---------------------------------------------------------------------------
class transform_passthrough_avi_v210 : public transform_v210
{
public:
    transform_passthrough_avi_v210(raw_frame* RawFrame, size_t x_offset, size_t y_offset, size_t w, size_t h) :
        transform_v210(RawFrame, x_offset, y_offset, w, h) {};
};

//***************************************************************************
// List
//***************************************************************************

//---------------------------------------------------------------------------
#define TRANSFORM_PIX_BEGIN(PIXSTYLE) \
case (pix_style::PIXSTYLE) : \
    switch (RawFrame->Flavor) \
    { \

#define TRANSFORM_PIX_END() \
    default: \
        return new transform_null; \
    } \

#define TRANSFORM_FLAVOR_BEGIN(FORMAT1, FORMAT2) \
case raw_frame::flavor::FORMAT2: \
    switch ((FORMAT1::flavor)RawFrame->Flavor_Private) \
    { \

#define TRANSFORM_FLAVOR_END() \
    default: \
        return new transform_null; \
    } \

#define TRANSFORM_CASE(TRANSFORMSTYLE, FORMAT, FLAVOR) \
case FORMAT::flavor::FLAVOR: \
    return new transform_##TRANSFORMSTYLE##_##FORMAT##_##FLAVOR(RawFrame, x_offset, y_offset, w, h); \

//---------------------------------------------------------------------------
transform_base* Transform_Init(raw_frame* RawFrame, pix_style PixStyle, size_t /*Bits*/, size_t x_offset, size_t y_offset, size_t w, size_t h)
{
    switch (PixStyle)
    {
    TRANSFORM_PIX_BEGIN(RGBA)
        TRANSFORM_FLAVOR_BEGIN(dpx, DPX)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_8)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_10_FilledA_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_10_FilledA_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_12_Packed_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_12_FilledA_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_12_FilledA_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_16_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGB_16_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_8)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_10_FilledA_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_10_FilledA_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_12_Packed_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_12_FilledA_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_12_FilledA_BE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_16_LE)
            TRANSFORM_CASE(jpeg2000rct, dpx, Raw_RGBA_16_BE)
        TRANSFORM_FLAVOR_END()
        TRANSFORM_FLAVOR_BEGIN(tiff, TIFF)
            TRANSFORM_CASE(jpeg2000rct, tiff, Raw_RGB_8_U)
            TRANSFORM_CASE(jpeg2000rct, tiff, Raw_RGB_16_U_LE)
            TRANSFORM_CASE(jpeg2000rct, tiff, Raw_RGB_16_U_BE)
            TRANSFORM_CASE(jpeg2000rct, tiff, Raw_RGBA_8_U)
            TRANSFORM_CASE(jpeg2000rct, tiff, Raw_RGBA_16_U_LE)
        TRANSFORM_FLAVOR_END()
        TRANSFORM_FLAVOR_BEGIN(exr, EXR)
            TRANSFORM_CASE(jpeg2000rct, exr, Raw_RGB_16)
        TRANSFORM_FLAVOR_END()
    TRANSFORM_PIX_END()
    TRANSFORM_PIX_BEGIN(YUVA)
        TRANSFORM_FLAVOR_BEGIN(dpx, DPX)
            TRANSFORM_CASE(passthrough, dpx, Raw_Y_8)
            TRANSFORM_CASE(passthrough, dpx, Raw_Y_10_FilledA_BE)
            TRANSFORM_CASE(passthrough, dpx, Raw_Y_10_FilledB_BE)
            TRANSFORM_CASE(passthrough, dpx, Raw_Y_12_Packed_BE)
            TRANSFORM_CASE(passthrough, dpx, Raw_Y_16_LE)
            TRANSFORM_CASE(passthrough, dpx, Raw_Y_16_BE)
        TRANSFORM_FLAVOR_END()
        TRANSFORM_FLAVOR_BEGIN(tiff, TIFF)
            TRANSFORM_CASE(passthrough, tiff, Raw_Y_8_U)
            TRANSFORM_CASE(passthrough, tiff, Raw_Y_16_U_LE)
            TRANSFORM_CASE(passthrough, tiff, Raw_Y_16_U_BE)
        TRANSFORM_FLAVOR_END()
        TRANSFORM_FLAVOR_BEGIN(avi, AVI)
            TRANSFORM_CASE(passthrough, avi, v210)
        TRANSFORM_FLAVOR_END()
    TRANSFORM_PIX_END()
    default:
        return new transform_null;
    }
}
