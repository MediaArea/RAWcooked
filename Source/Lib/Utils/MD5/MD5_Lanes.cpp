/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Utils/MD5/MD5_Lanes.h"
#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#if (defined(_M_X64) || defined(__x86_64__)) && !defined(HAVE_OPENSSL)
#include <immintrin.h>
#if defined(_MSC_VER)
#include <intrin.h>
#define TARGET_AVX2
#define TARGET_AVX512VL
#else
#define TARGET_AVX2 __attribute__((target("avx2")))
#define TARGET_AVX512VL __attribute__((target("avx2,avx512f,avx512vl")))
#endif
#define MD5_LANES_SIMD
#endif

//---------------------------------------------------------------------------
// Parts computed with scalar code, from Offset in the first part
static void MD5_Update_Parts(MD5_CTX* Ctx, const md5_part* Parts, size_t Count, size_t Offset)
{
    for (size_t i = 0; i < Count; i++, Offset = 0)
    {
        auto Data = Parts[i].Data + Offset;
        auto Size = Parts[i].Size - Offset;
        while (Size)
        {
            auto Size_Temp = Size < 0x40000000 ? Size : 0x40000000; // MD5_Update() size is an unsigned long
            MD5_Update(Ctx, Data, (unsigned long)Size_Temp);
            Data += Size_Temp;
            Size -= Size_Temp;
        }
    }
}

#ifdef MD5_LANES_SIMD

//---------------------------------------------------------------------------
// 0: none, 1: AVX2, 2: AVX-512 (F + VL)
static int SimdLevel()
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
            if ((Info[1] & (1 << 16)) && (Info[1] & (1 << 31)) && (XCR0 & 0xE6) == 0xE6) // AVX512F, AVX512VL, opmask and ZMM states
                return 2;
            return 1;
        #else
            __builtin_cpu_init();
            if (!__builtin_cpu_supports("avx2"))
                return 0;
            if (__builtin_cpu_supports("avx512f") && __builtin_cpu_supports("avx512vl"))
                return 2;
            return 1;
        #endif
    }();
    return Level;
}

//---------------------------------------------------------------------------
// 8 lanes of 32 bytes to 8 vectors of 8 lanes (8x8 transposition of 32-bit words)
#define MD5_LANES_TRANSPOSE(W, Data, Offset) \
    { \
        __m256i r0 = _mm256_loadu_si256((const __m256i*)(Data[0] + Offset)); \
        __m256i r1 = _mm256_loadu_si256((const __m256i*)(Data[1] + Offset)); \
        __m256i r2 = _mm256_loadu_si256((const __m256i*)(Data[2] + Offset)); \
        __m256i r3 = _mm256_loadu_si256((const __m256i*)(Data[3] + Offset)); \
        __m256i r4 = _mm256_loadu_si256((const __m256i*)(Data[4] + Offset)); \
        __m256i r5 = _mm256_loadu_si256((const __m256i*)(Data[5] + Offset)); \
        __m256i r6 = _mm256_loadu_si256((const __m256i*)(Data[6] + Offset)); \
        __m256i r7 = _mm256_loadu_si256((const __m256i*)(Data[7] + Offset)); \
        __m256i t0 = _mm256_unpacklo_epi32(r0, r1), t1 = _mm256_unpackhi_epi32(r0, r1); \
        __m256i t2 = _mm256_unpacklo_epi32(r2, r3), t3 = _mm256_unpackhi_epi32(r2, r3); \
        __m256i t4 = _mm256_unpacklo_epi32(r4, r5), t5 = _mm256_unpackhi_epi32(r4, r5); \
        __m256i t6 = _mm256_unpacklo_epi32(r6, r7), t7 = _mm256_unpackhi_epi32(r6, r7); \
        __m256i u0 = _mm256_unpacklo_epi64(t0, t2), u1 = _mm256_unpackhi_epi64(t0, t2); \
        __m256i u2 = _mm256_unpacklo_epi64(t1, t3), u3 = _mm256_unpackhi_epi64(t1, t3); \
        __m256i u4 = _mm256_unpacklo_epi64(t4, t6), u5 = _mm256_unpackhi_epi64(t4, t6); \
        __m256i u6 = _mm256_unpacklo_epi64(t5, t7), u7 = _mm256_unpackhi_epi64(t5, t7); \
        W[0] = _mm256_permute2x128_si256(u0, u4, 0x20); \
        W[1] = _mm256_permute2x128_si256(u1, u5, 0x20); \
        W[2] = _mm256_permute2x128_si256(u2, u6, 0x20); \
        W[3] = _mm256_permute2x128_si256(u3, u7, 0x20); \
        W[4] = _mm256_permute2x128_si256(u0, u4, 0x31); \
        W[5] = _mm256_permute2x128_si256(u1, u5, 0x31); \
        W[6] = _mm256_permute2x128_si256(u2, u6, 0x31); \
        W[7] = _mm256_permute2x128_si256(u3, u7, 0x31); \
    }

//---------------------------------------------------------------------------
// MD5 steps, same as md5.c
#define MD5_LANES_STEP(f, a, b, c, d, k, t, s) \
    a = _mm256_add_epi32(a, _mm256_add_epi32(W[k], _mm256_set1_epi32((int)t))); \
    a = _mm256_add_epi32(a, f(b, c, d)); \
    a = ROTL(a, s); \
    a = _mm256_add_epi32(a, b);
#define MD5_LANES_BODY(F, G, H, I) \
    MD5_LANES_STEP(F, a, b, c, d,  0, 0xd76aa478,  7) \
    MD5_LANES_STEP(F, d, a, b, c,  1, 0xe8c7b756, 12) \
    MD5_LANES_STEP(F, c, d, a, b,  2, 0x242070db, 17) \
    MD5_LANES_STEP(F, b, c, d, a,  3, 0xc1bdceee, 22) \
    MD5_LANES_STEP(F, a, b, c, d,  4, 0xf57c0faf,  7) \
    MD5_LANES_STEP(F, d, a, b, c,  5, 0x4787c62a, 12) \
    MD5_LANES_STEP(F, c, d, a, b,  6, 0xa8304613, 17) \
    MD5_LANES_STEP(F, b, c, d, a,  7, 0xfd469501, 22) \
    MD5_LANES_STEP(F, a, b, c, d,  8, 0x698098d8,  7) \
    MD5_LANES_STEP(F, d, a, b, c,  9, 0x8b44f7af, 12) \
    MD5_LANES_STEP(F, c, d, a, b, 10, 0xffff5bb1, 17) \
    MD5_LANES_STEP(F, b, c, d, a, 11, 0x895cd7be, 22) \
    MD5_LANES_STEP(F, a, b, c, d, 12, 0x6b901122,  7) \
    MD5_LANES_STEP(F, d, a, b, c, 13, 0xfd987193, 12) \
    MD5_LANES_STEP(F, c, d, a, b, 14, 0xa679438e, 17) \
    MD5_LANES_STEP(F, b, c, d, a, 15, 0x49b40821, 22) \
    MD5_LANES_STEP(G, a, b, c, d,  1, 0xf61e2562,  5) \
    MD5_LANES_STEP(G, d, a, b, c,  6, 0xc040b340,  9) \
    MD5_LANES_STEP(G, c, d, a, b, 11, 0x265e5a51, 14) \
    MD5_LANES_STEP(G, b, c, d, a,  0, 0xe9b6c7aa, 20) \
    MD5_LANES_STEP(G, a, b, c, d,  5, 0xd62f105d,  5) \
    MD5_LANES_STEP(G, d, a, b, c, 10, 0x02441453,  9) \
    MD5_LANES_STEP(G, c, d, a, b, 15, 0xd8a1e681, 14) \
    MD5_LANES_STEP(G, b, c, d, a,  4, 0xe7d3fbc8, 20) \
    MD5_LANES_STEP(G, a, b, c, d,  9, 0x21e1cde6,  5) \
    MD5_LANES_STEP(G, d, a, b, c, 14, 0xc33707d6,  9) \
    MD5_LANES_STEP(G, c, d, a, b,  3, 0xf4d50d87, 14) \
    MD5_LANES_STEP(G, b, c, d, a,  8, 0x455a14ed, 20) \
    MD5_LANES_STEP(G, a, b, c, d, 13, 0xa9e3e905,  5) \
    MD5_LANES_STEP(G, d, a, b, c,  2, 0xfcefa3f8,  9) \
    MD5_LANES_STEP(G, c, d, a, b,  7, 0x676f02d9, 14) \
    MD5_LANES_STEP(G, b, c, d, a, 12, 0x8d2a4c8a, 20) \
    MD5_LANES_STEP(H, a, b, c, d,  5, 0xfffa3942,  4) \
    MD5_LANES_STEP(H, d, a, b, c,  8, 0x8771f681, 11) \
    MD5_LANES_STEP(H, c, d, a, b, 11, 0x6d9d6122, 16) \
    MD5_LANES_STEP(H, b, c, d, a, 14, 0xfde5380c, 23) \
    MD5_LANES_STEP(H, a, b, c, d,  1, 0xa4beea44,  4) \
    MD5_LANES_STEP(H, d, a, b, c,  4, 0x4bdecfa9, 11) \
    MD5_LANES_STEP(H, c, d, a, b,  7, 0xf6bb4b60, 16) \
    MD5_LANES_STEP(H, b, c, d, a, 10, 0xbebfbc70, 23) \
    MD5_LANES_STEP(H, a, b, c, d, 13, 0x289b7ec6,  4) \
    MD5_LANES_STEP(H, d, a, b, c,  0, 0xeaa127fa, 11) \
    MD5_LANES_STEP(H, c, d, a, b,  3, 0xd4ef3085, 16) \
    MD5_LANES_STEP(H, b, c, d, a,  6, 0x04881d05, 23) \
    MD5_LANES_STEP(H, a, b, c, d,  9, 0xd9d4d039,  4) \
    MD5_LANES_STEP(H, d, a, b, c, 12, 0xe6db99e5, 11) \
    MD5_LANES_STEP(H, c, d, a, b, 15, 0x1fa27cf8, 16) \
    MD5_LANES_STEP(H, b, c, d, a,  2, 0xc4ac5665, 23) \
    MD5_LANES_STEP(I, a, b, c, d,  0, 0xf4292244,  6) \
    MD5_LANES_STEP(I, d, a, b, c,  7, 0x432aff97, 10) \
    MD5_LANES_STEP(I, c, d, a, b, 14, 0xab9423a7, 15) \
    MD5_LANES_STEP(I, b, c, d, a,  5, 0xfc93a039, 21) \
    MD5_LANES_STEP(I, a, b, c, d, 12, 0x655b59c3,  6) \
    MD5_LANES_STEP(I, d, a, b, c,  3, 0x8f0ccc92, 10) \
    MD5_LANES_STEP(I, c, d, a, b, 10, 0xffeff47d, 15) \
    MD5_LANES_STEP(I, b, c, d, a,  1, 0x85845dd1, 21) \
    MD5_LANES_STEP(I, a, b, c, d,  8, 0x6fa87e4f,  6) \
    MD5_LANES_STEP(I, d, a, b, c, 15, 0xfe2ce6e0, 10) \
    MD5_LANES_STEP(I, c, d, a, b,  6, 0xa3014314, 15) \
    MD5_LANES_STEP(I, b, c, d, a, 13, 0x4e0811a1, 21) \
    MD5_LANES_STEP(I, a, b, c, d,  4, 0xf7537e82,  6) \
    MD5_LANES_STEP(I, d, a, b, c, 11, 0xbd3af235, 10) \
    MD5_LANES_STEP(I, c, d, a, b,  2, 0x2ad7d2bb, 15) \
    MD5_LANES_STEP(I, b, c, d, a,  9, 0xeb86d391, 21)

//---------------------------------------------------------------------------
// 8 lanes, Blocks blocks of 64 bytes per lane, State: a, b, c, d of each lane
#define MD5_LANES_FUNCTION(NAME, TARGET) \
TARGET static void NAME(uint32_t State[4][8], const uint8_t* const* Data, size_t Blocks) \
{ \
    __m256i a = _mm256_loadu_si256((const __m256i*)State[0]); \
    __m256i b = _mm256_loadu_si256((const __m256i*)State[1]); \
    __m256i c = _mm256_loadu_si256((const __m256i*)State[2]); \
    __m256i d = _mm256_loadu_si256((const __m256i*)State[3]); \
    for (size_t n = 0; n < Blocks; n++) \
    { \
        __m256i W[16]; \
        MD5_LANES_TRANSPOSE(W, Data, n * 64); \
        MD5_LANES_TRANSPOSE((W + 8), Data, n * 64 + 32); \
        __m256i sa = a, sb = b, sc = c, sd = d; \
        MD5_LANES_BODY(F, G, H, I) \
        a = _mm256_add_epi32(a, sa); \
        b = _mm256_add_epi32(b, sb); \
        c = _mm256_add_epi32(c, sc); \
        d = _mm256_add_epi32(d, sd); \
    } \
    _mm256_storeu_si256((__m256i*)State[0], a); \
    _mm256_storeu_si256((__m256i*)State[1], b); \
    _mm256_storeu_si256((__m256i*)State[2], c); \
    _mm256_storeu_si256((__m256i*)State[3], d); \
}

// AVX2
#define F(x, y, z) _mm256_xor_si256(z, _mm256_and_si256(x, _mm256_xor_si256(y, z)))
#define G(x, y, z) _mm256_xor_si256(y, _mm256_and_si256(z, _mm256_xor_si256(x, y)))
#define H(x, y, z) _mm256_xor_si256(_mm256_xor_si256(x, y), z)
#define I(x, y, z) _mm256_xor_si256(y, _mm256_or_si256(x, _mm256_xor_si256(z, _mm256_set1_epi32(-1))))
#define ROTL(x, s) _mm256_or_si256(_mm256_slli_epi32(x, s), _mm256_srli_epi32(x, 32 - (s)))
MD5_LANES_FUNCTION(MD5_Lanes_AVX2, TARGET_AVX2)
#undef F
#undef G
#undef H
#undef I
#undef ROTL

// AVX-512VL: functions with 1 instruction (shorter dependency chain, so faster than scalar code even with 1 lane)
#define F(x, y, z) _mm256_ternarylogic_epi32(x, y, z, 0xCA)
#define G(x, y, z) _mm256_ternarylogic_epi32(x, y, z, 0xE4)
#define H(x, y, z) _mm256_ternarylogic_epi32(x, y, z, 0x96)
#define I(x, y, z) _mm256_ternarylogic_epi32(x, y, z, 0x39)
#define ROTL(x, s) _mm256_rol_epi32(x, s)
MD5_LANES_FUNCTION(MD5_Lanes_AVX512VL, TARGET_AVX512VL)
#undef F
#undef G
#undef H
#undef I
#undef ROTL

//---------------------------------------------------------------------------
namespace
{

const size_t Lanes_Max = 8;
const size_t Lanes_Min = 3; // Fewer jobs computed together are slower than scalar code in the threads of the callers (measured on AMD Zen 5: 1 lane 2x slower, 2 lanes using the same CPU and 2x slower in wall time)
const size_t Run_Max = 16384; // Max count of blocks per lane before new jobs can use free lanes
const size_t Batch_Min = Lanes_Min; // Count of jobs computed together if jobs accept to wait for other jobs
const auto Batch_Wait_Max = std::chrono::seconds(1);

struct md5_job
{
    MD5_CTX*                    Ctx;
    const md5_part*             Parts;
    size_t                      Count;
    size_t                      Part = 0; // Current part
    size_t                      Offset = 0; // In current part
    bool                        Batch = false; // Can wait for other jobs
    bool                        Done = false;
    bool                        Scalar = false; // Remaining data is computed by the caller with scalar code

    // Data up to the next full block (with scalar code), returns the count of full blocks available in the current part (0 if all data is computed)
    size_t Prepare()
    {
        for (;;)
        {
            if (Part >= Count)
                return 0;
            auto Remaining = Parts[Part].Size - Offset;
            if (!Remaining)
            {
                Part++;
                Offset = 0;
                continue;
            }
            auto Used = Ctx->lo & 63; // Bytes in the context buffer
            if (Used || Remaining < 64)
            {
                auto Size = Used ? 64 - Used : Remaining;
                if (Size > Remaining)
                    Size = Remaining;
                MD5_Update(Ctx, Parts[Part].Data + Offset, (unsigned long)Size);
                Offset += Size;
                continue;
            }
            return Remaining / 64;
        }
    }

    // Count of bytes computed by SIMD code
    void Computed(size_t Size)
    {
        Offset += Size;
        auto Saved_lo = Ctx->lo;
        if ((Ctx->lo = (Saved_lo + (MD5_u32plus)Size) & 0x1fffffff) < Saved_lo)
            Ctx->hi++;
        Ctx->hi += (MD5_u32plus)(Size >> 29);
    }

    // Remaining data with scalar code
    void Finish()
    {
        if (Part < Count)
            MD5_Update_Parts(Ctx, Parts + Part, Count - Part, Offset);
    }
};

//---------------------------------------------------------------------------
// Jobs from all threads computed in a single thread
class md5_lanes
{
public:
    md5_lanes() :
        Thread(&md5_lanes::Run, this)
    {
    }

    ~md5_lanes()
    {
        {
            std::lock_guard<std::mutex> Lock(Mutex);
            Stop = true;
        }
        Condition.notify_all();
        Thread.join();
    }

    void Compute(md5_job* Job)
    {
        {
            std::unique_lock<std::mutex> Lock(Mutex);
            Pending.push_back(Job);
            Condition.notify_all();
            Condition.wait(Lock, [&] { return Job->Done; });
        }
        if (Job->Scalar)
            Job->Finish(); // In the thread of the caller, in parallel with other callers
    }

    void Flush(bool Begin)
    {
        {
            std::lock_guard<std::mutex> Lock(Mutex);
            if (Begin)
                Flush_Count++;
            else
                Flush_Count--;
        }
        Condition.notify_all();
    }

private:
    void Run();

    std::mutex                  Mutex;
    std::condition_variable     Condition;
    std::deque<md5_job*>        Pending;
    size_t                      Flush_Count = 0;
    bool                        Stop = false;
    std::thread                 Thread;
};

//---------------------------------------------------------------------------
void md5_lanes::Run()
{
    auto Function = SimdLevel() >= 2 ? MD5_Lanes_AVX512VL : MD5_Lanes_AVX2;
    std::vector<md5_job*> Active;
    std::unique_lock<std::mutex> Lock(Mutex);
    for (;;)
    {
        // New jobs in free lanes, waiting for more jobs if no job is in progress and jobs accept to wait (lower CPU usage)
        Condition.wait(Lock, [&] { return !Pending.empty() || !Active.empty() || Stop; });
        if (Active.empty() && !Pending.empty())
        {
            auto Deadline = std::chrono::steady_clock::now() + Batch_Wait_Max;
            Condition.wait_until(Lock, Deadline, [&]
            {
                if (Stop || Flush_Count || Pending.size() >= Batch_Min)
                    return true;
                for (auto Job : Pending)
                    if (!Job->Batch)
                        return true;
                return false;
            });
        }
        while (Active.size() < Lanes_Max && !Pending.empty())
        {
            Active.push_back(Pending.front());
            Pending.pop_front();
        }
        if (Active.empty())
            return; // Stop
        if (Active.size() < Lanes_Min)
        {
            // Not enough jobs for being faster than scalar code, jobs are computed by their callers
            for (auto Job : Active)
            {
                Job->Scalar = true;
                Job->Done = true;
            }
            Active.clear();
            Condition.notify_all();
            continue;
        }
        Lock.unlock();

        // Data before the next full block and count of full blocks
        size_t Blocks = Run_Max;
        size_t Blocks_Lane[Lanes_Max];
        std::vector<md5_job*> Done;
        for (size_t l = 0; l < Active.size();)
        {
            Blocks_Lane[l] = Active[l]->Prepare();
            if (!Blocks_Lane[l])
            {
                Done.push_back(Active[l]);
                Active.erase(Active.begin() + l);
                continue;
            }
            if (Blocks > Blocks_Lane[l])
                Blocks = Blocks_Lane[l];
            l++;
        }

        // Full blocks of all lanes in parallel
        if (!Active.empty())
        {
            uint32_t State[4][Lanes_Max];
            const uint8_t* Data[Lanes_Max];
            for (size_t l = 0; l < Lanes_Max; l++)
            {
                auto Job = Active[l < Active.size() ? l : 0]; // Unused lanes compute the content of the 1st lane
                State[0][l] = Job->Ctx->a;
                State[1][l] = Job->Ctx->b;
                State[2][l] = Job->Ctx->c;
                State[3][l] = Job->Ctx->d;
                Data[l] = Job->Parts[Job->Part].Data + Job->Offset;
            }
            Function(State, Data, Blocks);
            for (size_t l = 0; l < Active.size(); l++)
            {
                auto Job = Active[l];
                Job->Ctx->a = State[0][l];
                Job->Ctx->b = State[1][l];
                Job->Ctx->c = State[2][l];
                Job->Ctx->d = State[3][l];
                Job->Computed(Blocks * 64);
            }
        }

        Lock.lock();
        if (!Done.empty())
        {
            for (auto Job : Done)
                Job->Done = true;
            Condition.notify_all();
        }
    }
}

} // namespace

#endif // MD5_LANES_SIMD

#ifdef MD5_LANES_SIMD
//---------------------------------------------------------------------------
static md5_lanes& Lanes()
{
    static md5_lanes Lanes_;
    return Lanes_;
}
#endif

//---------------------------------------------------------------------------
void MD5_Lanes_Flush(bool Begin)
{
    #ifdef MD5_LANES_SIMD
        if (SimdLevel())
            Lanes().Flush(Begin);
    #else
        (void)Begin;
    #endif
}

//---------------------------------------------------------------------------
void MD5_Update_Lanes(MD5_CTX* Ctx, const md5_part* Parts, size_t Count, bool Batch)
{
    #ifdef MD5_LANES_SIMD
        if (SimdLevel())
        {
            md5_job Job;
            Job.Ctx = Ctx;
            Job.Parts = Parts;
            Job.Count = Count;
            Job.Batch = Batch;
            Lanes().Compute(&Job);
            return;
        }
    #else
        (void)Batch;
    #endif

    // Without SIMD lanes, computed in the thread of the caller
    MD5_Update_Parts(Ctx, Parts, Count, 0);
}
