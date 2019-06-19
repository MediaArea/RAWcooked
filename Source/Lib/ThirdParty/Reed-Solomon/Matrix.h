/*  Copyright (c) MediaArea.net SARL.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef MatrixH
#define MatrixH
//---------------------------------------------------------------------------

 //---------------------------------------------------------------------------
#include <cstdint>
//---------------------------------------------------------------------------

/**
 * A matrix over the 8-bit Galois field.
 *
 * Note: input is not verified
 */

class Matrix
{
public:
    /**
     * Initialize a matrix of zeros.
     *
     * @param rows_minus1 The number of rows in the matrix minus 1.
     * @param columns_minus1 The number of columns in the matrix minus 1.
     */
    Matrix(uint8_t rows_minus1, uint8_t columns_minus1);
    Matrix(Matrix&& rhs);
    ~Matrix();

    /**
     * Returns pointer on one row.
     */
    inline const uint8_t* getRow(uint8_t r) const { return data[r]; }

    /**
     * Returns the value at row r, column c.
     */
    inline uint8_t get(uint8_t r, uint8_t c) const { return data[r][c]; }

    /**
     * Sets the value at row r, column c.
     */
    inline void set(uint8_t r, uint8_t c, uint8_t value) { data[r][c] = value; }

    /**
     * Returns a part of this matrix.
     */
    Matrix submatrix(uint8_t rmin, uint8_t cmin, uint8_t rmax, uint8_t cmax) const;

    /**
     * Multiplies this matrix (the one on the left) by another
     * matrix (the one on the right).
     */
    Matrix times(const Matrix& rhs) const;

    /**
     * Inverse this matrix.
     */
    void invert();

private:
    /**
     * Does the work of matrix inversion.
     *
     * Assumes that this is an r by 2r matrix.
     */
    void gaussianElimination();

    // Members
    const uint8_t               rowCountMinus1;
    const uint8_t               columnCountMinus1;
    uint8_t**                   data;
    uint8_t*                    dataPointer;

    // Forbidden
    Matrix(const Matrix& rhs) = delete;
    bool operator==(const Matrix& rhs) const = delete;
    bool operator!=(const Matrix& rhs) const = delete;
};

#endif
