/*  Copyright (c) MediaArea.net SARL.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

 //---------------------------------------------------------------------------
#ifndef ReedSolomonH
#define ReedSolomonH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Reed-Solomon/Matrix.h"
#include <vector>
using size_t = decltype(sizeof 1);
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#if defined (_MSC_VER)
#define __restrict__ __restrict
#endif
//---------------------------------------------------------------------------

/**
 * Reed-Solomon Coding over 8-bit values.
 */

class ReedSolomon
{
public:
    /**
     * Initializes a new encoder/decoder.
     */
    ReedSolomon(uint8_t dataShardCount_, uint8_t parityShardCount_);
    ~ReedSolomon();

    /**
     * Set SIMD (Single Instruction Multiple Data)
     * @param exponent Exponent of 2 of count of bytes in a single instruction
     *                 e.g. 4 for SSE (128-bit)
     *                 if set to -1, the highest exponent avalaible is selected
     * @return false if exponent is not supported, else true
     */
    static bool setSingleInstructionMultipleData(uint8_t exponent = -1);

    /**
     * Set processed buffer size 
     * @return false if size is not supported, else true
     */
    static bool setProcessedBufferSize(size_t bufferSize);

    /**
     * Set encoding order
     * @param order 0 = 
     *
     * @return false if order is not supported, else true
     */
    static bool setProcessedOrder(uint8_t order);

    /**
     * Returns false if constructor failed to initialize.
     */
    bool isValid() const { return !totalShardCountMinus1; }

    /**
     * Returns the number of data shards.
     */
    uint8_t getDataShardCount() const { return dataShardCount; }

    /**
     * Returns the number of parity shards.
     */
    uint8_t getParityShardCount() const { return parityShardCount; }

    /**
     * Returns the total number of shards.
     */
    uint16_t getTotalShardCount() const { return totalShardCountMinus1 + 1; }

    /**
     * Encodes parity for a set of data shards.
     *
     * @param shards An array containing data shards followed by parity shards.
     *               Each shard is a byte array, and they must all be the same
     *               size.
     * @param shardByteSize The number of bytes to encode in each shard.
     *
     */
    void encode(uint8_t* __restrict__ const* __restrict__ const shards, size_t shardByteSize) const;

    /**
     * Returns true if the parity shards contain the right data.
     *
     * @param shards An array containing data shards followed by parity shards.
     *               Each shard is a byte array, and they must all be the same
     *               size.
     * @param shardByteSize The number of bytes to check in each shard.
     */
    bool verify(uint8_t const* __restrict__ const* __restrict__ const shards, size_t shardByteSize) const;

    /**
     * Given a list of shards, some of which contain data, fills in the
     * ones that don't have data.
     *
     * Quickly does nothing if all of the shards are present.
     *
     * If any shards are missing (based on the flags in shardsPresent),
     * the data in those shards is recomputed and filled in.
     */
    bool repair(uint8_t* __restrict__ const* __restrict__ const shards, size_t shardByteSize, std::vector<bool> shardPresent) const;

private:
    void process(uint8_t const* __restrict__ const* __restrict__ const validMatrixRows, uint8_t const* __restrict__ const* __restrict__ const validDataShards, uint8_t* __restrict__ const* __restrict__ const newParityShards, uint8_t newParityShardCount, size_t shardByteSize) const;

private:
    // Members
    const uint8_t dataShardCount;
    const uint8_t parityShardCount;
    const uint8_t totalShardCountMinus1;

    /**
     * Rows from the matrix for encoding parity, each one as its own
     * byte array to allow for efficient access while encoding.
     */
    uint8_t const* __restrict* __restrict__ matrixRows;
    Matrix matrix;
};

#endif