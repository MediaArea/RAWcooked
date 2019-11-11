/*  Copyright (c) MediaArea.net SARL.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

/*
 * Based on https://www.backblaze.com/blog/reed-solomon
 * Based on https://github.com/Backblaze/JavaReedSolomon
 * Based on https://github.com/DrPizza/reed-solomon
 * Initial copyrights:
 * copyright 2015 Peter Bright, Backblaze.
 */

//---------------------------------------------------------------------------
#include "Reed-Solomon/Galois.h"
#include "Reed-Solomon/Matrix.h"
#include "Reed-Solomon/Reed-Solomon.h"
#include <cstring>
#if defined(__x86_64) || defined(_M_AMD64)
    #if defined(_MSC_VER)
        #include <intrin.h>
    #else
        #include <cpuid.h>
    #endif
    #include <immintrin.h>
#endif
using namespace std;

//---------------------------------------------------------------------------

/**
  * Create a Vandermonde matrix, which is guaranteed to have the
  * property that any subset of rows that forms a square matrix
  * is invertible.
  *
  * @param rows Number of rows in the result.
  * @param cols Number of columns in the result.
  * @return A Matrix.
  */
static Matrix vandermonde(uint8_t rows_minus1, uint8_t cols)
{
    uint8_t cols_minus1 = cols - 1;
    Matrix result{ rows_minus1, cols_minus1 };
    uint8_t r = 0;
    do
    {
        uint8_t c = 0;
        do
        {
            result.set(r, c, galois.exp(r, c));
        }
        while (c++ != cols_minus1);
    }
    while (r++ != rows_minus1);
    return result;
}

/**
 * Create the matrix to use for encoding, given the number of
 * data shards and the number of total shards.
 *
 * The top square of the matrix is guaranteed to be an identity
 * matrix, which means that the data shards are unchanged after
 * encoding.
 */
static Matrix buildMatrix(uint8_t dataShards, uint8_t totalShardsMinus1)
{
    if (!totalShardsMinus1)
        return Matrix(0, 0); // Fake one, it will be trashed

    // Start with a Vandermonde matrix.  This matrix would work,
    // in theory, but doesn't have the property that the data
    // shards are unchanged after encoding.
    Matrix v = vandermonde(totalShardsMinus1, dataShards);

    // Multiple by the inverse of the top square of the matrix.
    // This will make the top square be the identity matrix, but
    // preserve the property that any square subset of rows  is
    // invertible.
    Matrix top = v.submatrix(0, 0, dataShards, dataShards);
    top.invert();
    return v.times(top);
}

ReedSolomon::ReedSolomon(uint8_t dataShardCount_, uint8_t parityShardCount_) :
    dataShardCount{ dataShardCount_ },
    parityShardCount{ parityShardCount_ },
    totalShardCountMinus1{ !dataShardCount || !parityShardCount || dataShardCount + parityShardCount > 256 ? static_cast<uint8_t>(0) : static_cast<uint8_t>(dataShardCount + parityShardCount - 1) },
    matrixRows{ new const uint8_t * [parityShardCount] },
    matrix{ buildMatrix(dataShardCount, totalShardCountMinus1) }
{
    if (!totalShardCountMinus1)
        return;

    // Fill parity_rows
    for (uint8_t i = 0; i < parityShardCount; i++)
        matrixRows[i] = matrix.getRow(dataShardCount + i);
}

ReedSolomon::~ReedSolomon()
{
    delete[] matrixRows;
}

static size_t singleInstructionMultipleData = 0;
#if defined(__x86_64) || defined(_M_AMD64)
    #if defined(_MSC_VER)
        inline void cpuid(unsigned int cpuInfo[4], unsigned int id) { __cpuid((int*)cpuInfo, (int)id); }
    #else
        inline void cpuid(unsigned int cpuInfo[4], unsigned int id) { __get_cpuid(id, &cpuInfo[0], &cpuInfo[1], &cpuInfo[2], &cpuInfo[3]); }
    #endif
#endif
bool ReedSolomon::setSingleInstructionMultipleData(uint8_t exponent)
{
    if (exponent == (uint8_t )-1)
    {
        /*
        if (setSingleInstructionMultipleData(6))
            return true;
        if (setSingleInstructionMultipleData(5))
            return true;
        if (setSingleInstructionMultipleData(4))
            return true;
        */
        if (setSingleInstructionMultipleData(0))
            return true;
        return false;
    }

    #if defined(__x86_64) || defined(_M_AMD64)
        unsigned int cpuInfo[4];
    #endif

    switch (exponent)
    {
        case 0: break; // No SIMD, supported everywhere
        case 4: 
                #if defined(__x86_64) || defined(_M_AMD64)
                    break; // SSE3 everywhere
                #else
                    return false;
                #endif
        case 5: 
                #if defined(__x86_64) || defined(_M_AMD64)
                    cpuid(cpuInfo, 7);
                    if (cpuInfo[1] && (1 << 5))
                        break; // AVX2 supported
                #else
                    return false;
                #endif
        case 6: 
                #if defined(__x86_64) || defined(_M_AMD64)
                    cpuid(cpuInfo, 7);
                    if (cpuInfo[1] && ((1 << 16) | (1 << 30)))
                        break; // AVX512F + AVX512BW supported
                #else
                    return false;
                #endif
        default:
                return false; // Supported nowhere
    }

    singleInstructionMultipleData = exponent;
    return true;
}

static int processedBufferSize = 4096;
bool ReedSolomon::setProcessedBufferSize(size_t bufferSize)
{
    processedBufferSize = static_cast<int>(bufferSize);
    return true;
}

static size_t processedOrder = 0;
bool ReedSolomon::setProcessedOrder(uint8_t order)
{
    processedOrder = order;
    return true;
}

void ReedSolomon::encode(uint8_t* __restrict__ const* __restrict__ const shards, size_t shardByteSize) const
{
    // Build the array of data/parity shards
    auto dataShards = &shards[0];
    auto parityShards = &shards[dataShardCount];

    // Do the coding
    process(matrixRows, dataShards, parityShards, parityShardCount, shardByteSize);
}

bool ReedSolomon::verify(uint8_t const* __restrict__ const* __restrict__ const shards, size_t shardByteSize) const
{
    // Build the array of data/parity shards
    auto dataShards = &shards[0];
    auto parityShards = &shards[dataShardCount];

    // Create fake parity shards
    uint8_t* newParityShardsPointer = new uint8_t[parityShardCount * shardByteSize];
    memset(newParityShardsPointer, 0, parityShardCount * shardByteSize);
    uint8_t** newParityShards = new uint8_t * [parityShardCount];
    for (uint8_t i = 0; i < parityShardCount; i++)
        newParityShards[i] = &newParityShardsPointer[i * shardByteSize];

    // Do the coding as if there is no parity shards in the input
    process(matrixRows, dataShards, newParityShards, parityShardCount, shardByteSize);

    bool ok = true;
    for (uint8_t r = 0; r < parityShardCount; r++)
        if (memcmp(newParityShards[r], parityShards[r], shardByteSize))
            ok = false;

    delete[] newParityShards;
    delete[] newParityShardsPointer;
    return ok;

}

bool ReedSolomon::repair(uint8_t* __restrict__ const* __restrict__ const shards, size_t shardByteSize, std::vector<bool> shardPresent) const
{
    // Integrity check
    if (shardPresent.size() != totalShardCountMinus1 + 1)
        return false;
    
    // Quick check: are all of the shards present? If so, there's nothing to do.
    uint16_t number_present = 0;
    {
        uint8_t i = 0;
        do
        {
            if (shardPresent[i])
                number_present++;
        }
        while (i++ != totalShardCountMinus1);
    }
    if (number_present == totalShardCountMinus1 + 1)
    {
        // All of the shards data data. We don't need to do anything.
        return true;
    }
    if (number_present < dataShardCount)
    {
        // Not enough valid shards. We don't need to do anything.
        return false;
    }

    // Init invalid shard content to 0,
    // needed for process()
    {
        uint8_t i = 0;
        do
        {
            if (!shardPresent[i])
                memset(shards[i], 0, shardByteSize);
        }
        while (i++ != totalShardCountMinus1);
    }

    // Pull out the rows of the matrix that correspond to the
    // shards that we have and build a square matrix. This
    // matrix could be used to generate the shards that we have
    // from the original data.
    //
    // Also, pull out an array holding just the shards that
    // correspond to the rows of the submatrix. These shards
    // will be the input to the decoding process that re-creates
    // the missing data shards.
    Matrix dataDecodeMatrix{ static_cast<uint8_t>(dataShardCount - 1), static_cast<uint8_t>(dataShardCount - 1) };
    const uint8_t ** validDataShards = new const uint8_t * [dataShardCount];
    uint8_t validDataShardCount = 0;
    {
        uint8_t r = 0;
        do
        {
            if (shardPresent[r])
            {
                for (uint8_t c = 0; c < dataShardCount; ++c)
                    dataDecodeMatrix.set(validDataShardCount, c, matrix.get(r, c));
                validDataShards[validDataShardCount] = shards[r];
                validDataShardCount++;
            }
        }
        while (r++ <= totalShardCountMinus1 && validDataShardCount < dataShardCount);
    }

    // Invert the matrix, so we can go from the encoded shards
    // back to the original data.  Then pull out the row that
    // generates the shard that we want to decode.  Note that
    // since this matrix maps back to the orginal data, it can
    // be used to create a data shard, but not a parity shard.
    dataDecodeMatrix.invert();

    // Re-create any data shards that were missing.
    //
    // The input to the coding is all of the shards we actually
    // have, and the output is the missing data shards. The computation
    // is done using the special decode matrix we just built.
    uint8_t** newParityShards = new uint8_t * [parityShardCount];
    const uint8_t** validMatrixRows = new const uint8_t * [parityShardCount];
    uint8_t newParityShardCount = 0;
    for (uint8_t i = 0; i < dataShardCount; i++)
    {
        if (!shardPresent[i])
        {
            newParityShards[newParityShardCount] = shards[i];
            validMatrixRows[newParityShardCount] = dataDecodeMatrix.getRow(i);
            newParityShardCount++;
        }
    }
    process(validMatrixRows, validDataShards, newParityShards, newParityShardCount, shardByteSize);

    // Now that we have all of the data shards intact, we can
    // compute any of the parity that is missing.
    //
    // The input to the coding is ALL of the data shards, including
    // any that we just calculated. The output is whichever of the
    // data shards were missing.
    newParityShardCount = 0;
    {
        uint16_t i = dataShardCount;
        do
        {
            if (!shardPresent[i])
            {
                newParityShards[newParityShardCount] = shards[i];
                validMatrixRows[newParityShardCount] = matrixRows[i - dataShardCount];
                newParityShardCount++;
            }
        }
        while (i++ != totalShardCountMinus1);
    }
    const uint8_t* __restrict__ * __restrict__ dataShards = const_cast<const uint8_t * __restrict__ * __restrict__>(&shards[0]);
    process(validMatrixRows, dataShards, newParityShards, newParityShardCount, shardByteSize);

    delete[] validDataShards;
    delete[] newParityShards;
    delete[] validMatrixRows;

    return true;
}

/**
 * Multiplies a subset of rows from a coding matrix by a full set of
 * input shards to produce some output shards.
 *
 * @param validMatrixRows The rows from the matrix to use.
 * @param validDataShards An array of byte arrays, each of which is one input shard.
 *               The inputs array may have extra buffers after the ones
 *               that are used.  They will be ignored.  The number of
 *               inputs used is determined by the length of the
 *               each matrix row.
 * @param newParityShards Byte arrays where the computed shards are stored.  The
 *                outputs array may also have extra, unused, elements
 *                at the end.  The number of outputs computed, and the
 *                number of matrix rows used, is determined by
 *                outputCount.
 * @param newParityShardCount The number of newParityShards to compute.
 * @param shardByteSize The number of bytes to process.
 */
void ReedSolomon::process(uint8_t const* __restrict__ const* __restrict__ const validMatrixRows, uint8_t const* __restrict__ const* __restrict__ const validDataShards, uint8_t* __restrict__ const* __restrict__ const newParityShards, uint8_t newParityShardCount, size_t shardByteSize) const
{
    size_t loop1, loop2, loop3;
    switch (processedOrder)
    {
        case 3 :
                loop1 = shardByteSize / processedBufferSize;
                shardByteSize = processedBufferSize;
                loop2 = dataShardCount;
                loop3 = newParityShardCount;
                break;
        case 2:
                loop1 = shardByteSize / processedBufferSize;
                shardByteSize = processedBufferSize;
                loop2 = newParityShardCount;
                loop3 = dataShardCount;
                break;
        case 1 :
                loop1 = dataShardCount;
                loop2 = newParityShardCount;
                loop3 = 1;
                break;
        default :
                loop1 = newParityShardCount;
                loop2 = dataShardCount;
                loop3 = 1;
                break;
    }

    for (int l1 = 0; l1 < loop1; l1++)
        for (int l2 = 0; l2 < loop2; l2++)
            for (int l3 = 0; l3 < loop3; l3++)
            {
            int r, c, o;
            switch (processedOrder)
            {
                case 3 :
                        r = l3;
                        c = l2;
                        o = l1 * processedBufferSize;
                        break;
                case 2:
                        r = l2;
                        c = l3;
                        o = l1 * processedBufferSize;
                        break;
                case 1 :
                        r = l2;
                        c = l1;
                        o = 0;
                        break;
                default :
                        r = l1;
                        c = l2;
                        o = 0;
                        break;
            }

            uint8_t matrixValue = validMatrixRows[r][c];
            const uint8_t* __restrict__ dataShard = validDataShards[c] + o;
            uint8_t * __restrict__ const newParityShard = newParityShards[r] + o;

            if (singleInstructionMultipleData == 0)
            {
                for (size_t i = 0; i < shardByteSize; i++)
                    newParityShard[i] ^= galois.MultiplicationTable[matrixValue][dataShard[i]];
                continue;
            }

            /*
            size_t end = shardByteSize & (~((size_t(1) << singleInstructionMultipleData) - 1));

            if (singleInstructionMultipleData == 4)
            {
                const __m128i* __restrict__ input_ptr = reinterpret_cast<const __m128i*>(&dataShard[0]);
                __m128i* __restrict__ output_ptr = reinterpret_cast<__m128i*>(&newParityShard[0]);
                const __m128i* __restrict__ input_end = input_ptr + end / 16;

                const __m128i low_table = _mm_loadu_si128(reinterpret_cast<const __m128i*>(galois.MultiplicationTableSplit[matrixValue].Low));
                const __m128i high_table = _mm_loadu_si128(reinterpret_cast<const __m128i*>(galois.MultiplicationTableSplit[matrixValue].High));
                const __m128i mask = _mm_set1_epi8(0x0F);
                while (input_ptr < input_end)
                {
                    __m128i input = _mm_loadu_si128(input_ptr++);
                    __m128i low_indices = _mm_and_si128(input, mask);
                    __m128i low_parts = _mm_shuffle_epi8(low_table, low_indices);
                    __m128i high_indices = _mm_and_si128(_mm_srli_epi32(input, 4), mask); // Equivalent to _mm_srli_epi8(input, 4);
                    __m128i high_parts = _mm_shuffle_epi8(high_table, high_indices);
                    __m128i intermediate = _mm_xor_si128(low_parts, high_parts);
                    _mm_storeu_si128(output_ptr, _mm_xor_si128(_mm_loadu_si128(output_ptr), intermediate)); // output = output XOR intermediate
                    output_ptr++;
                }
            }

            if (singleInstructionMultipleData == 5)
            {
                const __m256i* __restrict__ input_ptr = reinterpret_cast<const __m256i*>(&dataShard[0]);
                __m256i* __restrict__ output_ptr = reinterpret_cast<__m256i*>(&newParityShard[0]);
                const __m256i* __restrict__ input_end = input_ptr + end / 32;

                const __m256i low_table = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(galois.MultiplicationTableSplit[matrixValue].Low));
                const __m256i high_table = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(galois.MultiplicationTableSplit[matrixValue].High));
                const __m256i mask = _mm256_set1_epi8(0x0F);
                while (input_ptr < input_end)
                {
                    __m256i input = _mm256_loadu_si256(input_ptr++);
                    __m256i low_indices = _mm256_and_si256(input, mask);
                    __m256i low_parts = _mm256_shuffle_epi8(low_table, low_indices);
                    __m256i high_indices = _mm256_and_si256(_mm256_srli_epi32(input, 4), mask); // Equivalent to _mm256_srli_epi8(input, 4);
                    __m256i high_parts = _mm256_shuffle_epi8(high_table, high_indices);
                    __m256i intermediate = _mm256_xor_si256(low_parts, high_parts);
                    _mm256_storeu_si256(output_ptr, _mm256_xor_si256(_mm256_loadu_si256(output_ptr), intermediate)); // output = output XOR intermediate
                    output_ptr++;
                }
            }

            if (singleInstructionMultipleData == 6)
            {
                const __m512i* __restrict__ input_ptr = reinterpret_cast<const __m512i*>(&dataShard[0]);
                __m512i* __restrict__ output_ptr = reinterpret_cast<__m512i*>(&newParityShard[0]);
                const __m512i* __restrict__ input_end = input_ptr + end / 64;

                const __m512i low_table = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(galois.MultiplicationTableSplit[matrixValue].Low));
                const __m512i high_table = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(galois.MultiplicationTableSplit[matrixValue].High));
                const __m512i mask = _mm512_set1_epi8(0x0F);
                while (input_ptr < input_end)
                {
                    __m512i input = _mm512_loadu_si512(input_ptr++);
                    __m512i low_indices = _mm512_and_si512(input, mask);
                    __m512i low_parts = _mm512_shuffle_epi8(low_table, low_indices);
                    __m512i high_indices = _mm512_and_si512(_mm512_srli_epi32(input, 4), mask); // Equivalent to _mm512_srli_epi8(input, 4);
                    __m512i high_parts = _mm512_shuffle_epi8(high_table, high_indices);
                    __m512i intermediate = _mm512_xor_si512(low_parts, high_parts);
                    _mm512_storeu_si512(output_ptr, _mm512_xor_si512(_mm512_loadu_si512(output_ptr), intermediate)); // output = output XOR intermediate
                    output_ptr++;
                }
            }

            for (; end < shardByteSize; end++)
                newParityShard[end] ^= galois.MultiplicationTable[matrixValue][dataShard[end]];
            */
        }
}
