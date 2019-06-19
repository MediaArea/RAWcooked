/*  Copyright (c) MediaArea.net SARL.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

 //---------------------------------------------------------------------------
#ifndef GaloisH
#define GaloisH
//---------------------------------------------------------------------------

/**
 * 8-bit Galois Field
 */

 //---------------------------------------------------------------------------
#include <cstdint>
//---------------------------------------------------------------------------

class Galois
{
public:
    Galois();

    /**
     * Multiplies to elements of the field.
     */
    uint8_t multiply(uint8_t a, uint8_t b);

    /**
     * Inverse of multiplication.
     */
    uint8_t divide(uint8_t a, uint8_t b);

    /**
     * Computes a**n.
     *
     * The result will be the same as multiplying a times itself n times.
     *
     * @param a A member of the field.
     * @param n A plain-old integer.
     * @return The result of multiplying a by itself n times.
     */
    uint8_t exp(uint8_t a, uint8_t n);

    // Members
    uint8_t MultiplicationTable[256][256];
    struct multiplication_table_split
    {
        uint8_t High[4 * 16]; // 4 high bits
        uint8_t Low[4 * 16]; // 4 low bits
    };
    multiplication_table_split MultiplicationTableSplit[256];
};

extern Galois galois;

#endif
