/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Input_Base.h"
#include <cmath>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
static const char* Buffer_Overflow = "at least 1 file is smaller than expected.";

//---------------------------------------------------------------------------
static const char* ErrorTypes_Before[] =
{
    "Error: untested ",
    "Error: ",
};

//---------------------------------------------------------------------------
static const char* ErrorTypes_After[] =
{
    ".\nPlease contact info@mediaarea.net if you want support of such content.",
    ".",
};

//---------------------------------------------------------------------------
input_base::input_base() :
    Buffer(NULL),
    Buffer_Size(0),
    Buffer_Offset(0),
    IsDetected(false),
    Error_Message(NULL)
{
}

//---------------------------------------------------------------------------
input_base::~input_base()
{
}

//---------------------------------------------------------------------------
uint8_t input_base::Get_X1()
{
    if (Buffer_Offset >= Buffer_Size)
    {
        Invalid(Buffer_Overflow);
        return 0;
    }

    uint8_t ToReturn = Buffer[Buffer_Offset];
    Buffer_Offset ++;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint16_t input_base::Get_L2()
{
    if (Buffer_Offset + 1 >= Buffer_Size)
    {
        Invalid(Buffer_Overflow);
        return 0;
    }

    uint16_t ToReturn = Buffer[Buffer_Offset + 0] | (Buffer[Buffer_Offset + 1] << 8);
    Buffer_Offset += 2;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint16_t input_base::Get_B2()
{
    if (Buffer_Offset + 2 >= Buffer_Size)
    {
        Invalid(Buffer_Overflow);
        return 0;
    }

    uint16_t ToReturn = (Buffer[Buffer_Offset + 0] << 8) | Buffer[Buffer_Offset + 1];
    Buffer_Offset += 2;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint32_t input_base::Get_L4()
{
    if (Buffer_Offset + 3 >= Buffer_Size)
    {
        Invalid(Buffer_Overflow);
        return 0;
    }

    uint32_t ToReturn = Buffer[Buffer_Offset + 0] | (Buffer[Buffer_Offset + 1] << 8) | (Buffer[Buffer_Offset + 2] << 16) | (Buffer[Buffer_Offset + 3] << 24);
    Buffer_Offset += 4;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint32_t input_base::Get_B4()
{
    if (Buffer_Offset + 3 >= Buffer_Size)
    {
        Invalid(Buffer_Overflow);
        return 0;
    }

    uint32_t ToReturn = (Buffer[Buffer_Offset + 0] << 24) | (Buffer[Buffer_Offset + 1] << 16) | (Buffer[Buffer_Offset + 2] << 8) | Buffer[Buffer_Offset + 3];
    Buffer_Offset += 4;
    return ToReturn;
}

//---------------------------------------------------------------------------
uint64_t input_base::Get_B8()
{
    if (Buffer_Offset + 7 >= Buffer_Size)
    {
        Invalid(Buffer_Overflow);
        return 0;
    }

    uint64_t ToReturn = ((uint64_t)Buffer[Buffer_Offset + 0] << 56) | ((uint64_t)Buffer[Buffer_Offset + 1] << 48) | ((uint64_t)Buffer[Buffer_Offset + 2] << 40) | ((uint64_t)Buffer[Buffer_Offset + 3] << 32) | (Buffer[Buffer_Offset + 4] << 24) | (Buffer[Buffer_Offset + 5] << 16) | (Buffer[Buffer_Offset + 6] << 8) | Buffer[Buffer_Offset + 7];
    Buffer_Offset += 8;
    return ToReturn;
}

//---------------------------------------------------------------------------
long double input_base::Get_BF10()
{
    if (Buffer_Offset + 10 >= Buffer_Size)
    {
        Invalid(Buffer_Overflow);
        return 0;
    }

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
    if (Buffer_Offset + 3 >= Buffer_Size)
    {
        Invalid(Buffer_Overflow);
        return 0;
    }

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
    if (Buffer_Offset >= Buffer_Size)
    {
        Invalid(Buffer_Overflow);
        return 0;
    }

    uint32_t ToReturn = Buffer[Buffer_Offset];
    uint64_t s = 0;
    while (!(ToReturn&(((uint64_t)1) << (7 - s))))
        s++;
    ToReturn ^= (((uint64_t)1) << (7 - s));
    if (Buffer_Offset + s >= Buffer_Size)
    {
        Invalid(Buffer_Overflow);
        return 0;
    }
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
const char* input_base::ErrorMessage()
{
    return Error_Message;
}

//---------------------------------------------------------------------------
const char* input_base::ErrorType_Before()
{
    return ErrorTypes_Before[Error_Type];
}

//---------------------------------------------------------------------------
const char* input_base::ErrorType_After()
{
    return ErrorTypes_After[Error_Type];
}

//---------------------------------------------------------------------------
bool input_base::Unsupported(const char* Message)
{
    if (Error_Message)
        return true; // Already filled, currently ignoring the second message

    Error_Message = Message;
    Error_Type = Error_Unsupported;
    return true;
}

//---------------------------------------------------------------------------
bool input_base::Invalid(const char* Message)
{
    if (Error_Message)
        return true; // Already filled, currently ignoring the second message

    Error_Message = Message;
    Error_Type = Error_Invalid;
    return true;
}

//---------------------------------------------------------------------------
uncompressed::uncompressed(parser ParserCode_, bool IsSequence_) :
    ParserCode(ParserCode_),
    RAWcooked(NULL),
    IsSequence(IsSequence_),
    Flavor((uint8_t)-1)
{
}

//---------------------------------------------------------------------------
uncompressed::~uncompressed()
{
}

