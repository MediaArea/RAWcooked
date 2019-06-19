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
#include <cstring>
#include <utility>
#include <stdexcept>
using namespace std;
//---------------------------------------------------------------------------

Matrix::Matrix(uint8_t rows_minus1, uint8_t columns_minus1) :
    rowCountMinus1{ rows_minus1 },
    columnCountMinus1{ columns_minus1 },
    data{ new uint8_t * [rowCountMinus1 + 1] },
    dataPointer{ new uint8_t[(rowCountMinus1 + 1) * (columnCountMinus1 + 1) * 2] } // Twice the needed size in order to anticipate invert()
{
    uint8_t* dataTemp = dataPointer;
    uint16_t columnCount = columnCountMinus1 + 1;
    uint8_t r = 0;
    do
    {
        data[r] = dataTemp;
        dataTemp += columnCount;
    }
    while (r++ != rowCountMinus1);
}

// Move
Matrix::Matrix(Matrix&& rhs) :
    rowCountMinus1{ rhs.rowCountMinus1 },
    columnCountMinus1{ rhs.columnCountMinus1 },
    dataPointer{ rhs.dataPointer },
    data{ rhs.data }
{
    rhs.dataPointer = nullptr;
    rhs.data = nullptr;
}

Matrix::~Matrix()
{
    delete[] data;
    delete[] dataPointer;
}

Matrix Matrix::submatrix(uint8_t rmin, uint8_t cmin, uint8_t rmax, uint8_t cmax) const
{
    Matrix result{ static_cast<uint8_t>(rmax - rmin - 1), static_cast<uint8_t>(cmax - cmin - 1) };
    for (uint8_t r = rmin; r < rmax; ++r)
        for (uint8_t c = cmin; c < cmax; ++c)
            result.data[r - rmin][c - cmin] = data[r][c];
    return result;
}

Matrix Matrix::times(const Matrix & rhs) const
{
    Matrix result{ rowCountMinus1, rhs.columnCountMinus1 };
    uint8_t r = 0;
    do
    {
        uint8_t c = 0;
        do
        {
            uint8_t value = 0;
            uint8_t i = 0;
            do
            {
                value ^= galois.multiply(data[r][i], rhs.data[i][c]);
            }
            while (i++ != columnCountMinus1);
            result.data[r][c] = value;
        }
        while (c++ != rhs.columnCountMinus1);
    }
    while (r++ != rowCountMinus1);
    return result;
}

void Matrix::invert()
{
    // Augmenting this matrix with
    // an identity matrix on the right.
    const uint16_t size = (rowCountMinus1 + 1) * (columnCountMinus1 + 1);
    uint8_t * dataTemp = dataPointer + size;
    uint8_t * dataTemp2 = dataTemp + size;
    uint16_t r = rowCountMinus1 + 1;
    uint16_t columnCount = columnCountMinus1 + 1;
    do
    {
        dataTemp2 -= columnCount;
        memset(dataTemp2, 0, columnCount);
        dataTemp2[--r] = 1;
        dataTemp2 -= columnCount;
        dataTemp -= columnCount;
        memcpy(dataTemp2, dataTemp, columnCount);
        data[r] = dataTemp2;
    }
    while (r);

    // Do Gaussian elimination to transform the left half into
    // an identity matrix.
    gaussianElimination();

    // The right half is now the inverse.
    dataTemp2 += columnCount;
    uint16_t columnCount2 = columnCount * 2;
    while (r <= rowCountMinus1)
    {
        memcpy(dataTemp, dataTemp2, columnCount);
        data[r++] = dataTemp;
        dataTemp += columnCount;
        dataTemp2 += columnCount2;
    }
}

void Matrix::gaussianElimination()
{
    const uint16_t columnCount2 = (columnCountMinus1 + 1) * 2;

    // Clear out the part below the main diagonal and scale the main
    // diagonal to be 1.
    uint8_t r = 0;
    do
    {
        // If the element on the diagonal is 0, find a row below
        // that has a non-zero and swap them.
        if (!data[r][r])
        {
            uint8_t rowBelow = r + 1;
            do
            {
                if (data[rowBelow][r])
                {
                    //Swap
                    for (uint16_t c = 0; c < columnCount2; c++)
                        swap(data[r][c], data[rowBelow][c]);
                }
            }
            while (rowBelow++ != rowCountMinus1);

            // If we couldn't find one, the matrix is singular.
            if (!data[r][r])
            {
                throw runtime_error("matrix is singular");
            }
        }

        // Scale to 1.
        if (data[r][r] != 1)
        {
            uint8_t scale = galois.divide(1, data[r][r]);
            for (uint16_t c = 0; c < columnCount2; ++c)
                data[r][c] = galois.multiply(data[r][c], scale);
        }

        // Make everything below the 1 be a 0 by subtracting
        // a multiple of it. (Subtraction and addition are
        // both exclusive or in the Galois field.)
        uint8_t r2 = 0;
        do
        {
            if (r2 == r)
                continue;
            uint8_t scale = data[r2][r];
            if (scale)
            {
                for (uint16_t c = 0; c < columnCount2; c++)
                    data[r2][c] ^= galois.multiply(scale, data[r][c]);
            }
        }
        while (r2++ != rowCountMinus1);
    }
    while (r++ != rowCountMinus1);
}
