/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/FileIO.h"
#include "Lib/Input_Base.h"
#include <cmath>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
input_base::input_base(parser ParserCode_) :
    ParserCode(ParserCode_)
{
}

//---------------------------------------------------------------------------
input_base::input_base(errors* Errors_Source, parser ParserCode_) :
    ParserCode(ParserCode_),
    Errors(Errors_Source)
{
}

//---------------------------------------------------------------------------
input_base::~input_base()
{
}

//---------------------------------------------------------------------------
bool input_base::Parse(unsigned char* Buffer_Source, size_t Buffer_Size_Source)
{
    ClearInfo();
    FileMap = NULL;
    Buffer = Buffer_Source;
    Buffer_Size = Buffer_Size_Source;

    ParseBuffer();

    return !IsDetected();
}

//---------------------------------------------------------------------------
bool input_base::Parse(filemap& FileMap_Source)
{
    ClearInfo();
    FileMap = &FileMap_Source;
    Buffer = FileMap_Source.Buffer;
    Buffer_Size = FileMap_Source.Buffer_Size;

    ParseBuffer();

    return !IsDetected();
}

//---------------------------------------------------------------------------
// Common
#define TEST_BUFFEROVERFLOW(_SIZE) \
    if (Buffer_Offset + _SIZE - 1 >= Buffer_Size) \
    { \
        SetBufferOverflow(); \
        return 0; \
    } \

//---------------------------------------------------------------------------
uint8_t input_base::Get_X1()
{
    TEST_BUFFEROVERFLOW(1);

    uint8_t ToReturn = Buffer[Buffer_Offset];
    Buffer_Offset ++;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint16_t input_base::Get_L2()
{
    TEST_BUFFEROVERFLOW(2);

    uint16_t ToReturn = Buffer[Buffer_Offset + 0] | (Buffer[Buffer_Offset + 1] << 8);
    Buffer_Offset += 2;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint16_t input_base::Get_B2()
{
    TEST_BUFFEROVERFLOW(2);

    uint16_t ToReturn = (Buffer[Buffer_Offset + 0] << 8) | Buffer[Buffer_Offset + 1];
    Buffer_Offset += 2;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint32_t input_base::Get_L4()
{
    TEST_BUFFEROVERFLOW(4);

    uint32_t ToReturn = Buffer[Buffer_Offset + 0] | (Buffer[Buffer_Offset + 1] << 8) | (Buffer[Buffer_Offset + 2] << 16) | (Buffer[Buffer_Offset + 3] << 24);
    Buffer_Offset += 4;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint32_t input_base::Get_B4()
{
    TEST_BUFFEROVERFLOW(4);

    uint32_t ToReturn = (Buffer[Buffer_Offset + 0] << 24) | (Buffer[Buffer_Offset + 1] << 16) | (Buffer[Buffer_Offset + 2] << 8) | Buffer[Buffer_Offset + 3];
    Buffer_Offset += 4;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint64_t input_base::Get_B8()
{
    TEST_BUFFEROVERFLOW(8);

    uint64_t ToReturn = ((uint64_t)Buffer[Buffer_Offset + 0] << 56) | ((uint64_t)Buffer[Buffer_Offset + 1] << 48) | ((uint64_t)Buffer[Buffer_Offset + 2] << 40) | ((uint64_t)Buffer[Buffer_Offset + 3] << 32) | (Buffer[Buffer_Offset + 4] << 24) | (Buffer[Buffer_Offset + 5] << 16) | (Buffer[Buffer_Offset + 6] << 8) | Buffer[Buffer_Offset + 7];
    Buffer_Offset += 8;
    return ToReturn;
}

//---------------------------------------------------------------------------
long double input_base::Get_BF10()
{
    TEST_BUFFEROVERFLOW(10);

    //sign          1 bit
    //exponent     15 bit
    //integer?      1 bit
    //significand  63 bit

    //Retrieving data
    uint16_t Integer1 = Get_B2();
    uint64_t Integer2 = Get_B8();

    //Retrieving elements
    bool     Sign = (Integer1 & 0x8000) ? true : false;
    uint16_t Exponent = Integer1 & 0x7FFF;
    uint64_t Mantissa = Integer2 & 0x7FFFFFFFFFFFFFFFLL; //Only 63 bits, 1 most significant bit is explicit
    //Some computing
    if (Exponent == 0 || Exponent == 0x7FFF)
        return 0; //These are denormalised numbers, NANs, and other horrible things
    Exponent -= 0x3FFF; //Bias
    long double ToReturn = (((long double)Mantissa) / 9223372036854775808.0 + 1.0)*std::pow((float)2, (int)Exponent); //(1+Mantissa) * 2^Exponent
    if (Sign)
        ToReturn = -ToReturn;

    return (long double)ToReturn;
}

//---------------------------------------------------------------------------
double input_base::Get_XF4()
{
    TEST_BUFFEROVERFLOW(4);

    // sign          1 bit
    // exponent      8 bit
    // significand  23 bit

    // Retrieving data
    uint32_t Integer = Get_X4();

    // Retrieving elements
    bool     Sign = (Integer >> 31) ? true : false;
    uint32_t Exponent = (Integer >> 23) & 0xFF;
    uint32_t Mantissa = Integer & 0x007FFFFF;

    // Some computing
    if (Exponent == 0 || Exponent == 0xFF)
        return 0; // These are denormalised numbers, NANs, and other horrible things
    Exponent -= 0x7F; // Bias
    double Answer = (((double)Mantissa) / 8388608 + 1.0)*std::pow((double)2, (int)Exponent); // (1+Mantissa) * 2^Exponent
    if (Sign)
        Answer = -Answer;

    return Answer;
}

//---------------------------------------------------------------------------
uint64_t input_base::Get_EB()
{
    TEST_BUFFEROVERFLOW(1);

    uint64_t ToReturn = Buffer[Buffer_Offset];
    uint64_t s = 0;
    while (!(ToReturn&(((uint64_t)1) << (7 - s))))
        s++;
    ToReturn ^= (((uint64_t)1) << (7 - s));
    TEST_BUFFEROVERFLOW(1 + s);
    while (s)
    {
        ToReturn <<= 8;
        Buffer_Offset++;
        s--;
        ToReturn |= Buffer[Buffer_Offset];
    }
    Buffer_Offset++;

    return ToReturn;
}

//---------------------------------------------------------------------------
void input_base::Error(error::type Type, error::generic::code Code)
{
    if (HasBufferOverflow())
        return; // Next errors are not real, due to buffer overflow
    if (!HasErrors())
        SetErrors();
    if (Errors)
        Errors->Error(ParserCode, Type, Code);
}

//---------------------------------------------------------------------------
uncompressed::uncompressed(bool IsSequence_) :
    RAWcooked(NULL),
    IsSequence(IsSequence_),
    Flavor((flavor)-1)
{
}

//---------------------------------------------------------------------------
uncompressed::~uncompressed()
{
}

