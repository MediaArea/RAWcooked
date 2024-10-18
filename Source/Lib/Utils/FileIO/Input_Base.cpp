/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Utils/FileIO/FileIO.h"
#include "Lib/Uncompressed/HashSum/HashSum.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
extern "C"
{
#include "md5.h"
}
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
bool input_base::Parse(filemap* FileMap_Source, const buffer_view& Buffer_Source, size_t FileSize_Source)
{
    FileMap = FileMap_Source;
    FileMap2 = nullptr;
    FileSize = FileSize_Source == (size_t)-1 ? Buffer_Source.Size() : FileSize_Source;
    Buffer = Buffer_Source;
    HashComputed = false;

    ParseBuffer();
    if (IsDetected())
        Hash();

    return !IsDetected();
}

//---------------------------------------------------------------------------
void input_base::Hash()
{
    if (!Actions[Action_Hash] || HashComputed)
        return;

    // MD5    
    {
        MD5_CTX MD5;
        MD5_Init(&MD5);

        size_t Offset = 0;
        while (Offset < Buffer.Size())
        {
            unsigned long Size_Temp;
            if (Buffer.Size() - Offset >= (unsigned long)-1) // MD5_Update() accepts only unsigned longs
                Size_Temp = (unsigned long)-1;
            else
                Size_Temp = (unsigned long)(Buffer.Size() - Offset);
            MD5_Update(&MD5, Buffer.Data(), Size_Temp);
            Offset += Size_Temp;
        }

        MD5_Final(HashValue.data(), &MD5);
        if (Hashes&& FileName && !FileName->empty())
            Hashes->FromFile(*FileName, HashValue);
    }
    HashComputed = true;
}

//---------------------------------------------------------------------------
// Common
#define TEST_BUFFEROVERFLOW(_SIZE) \
    if (Buffer_Offset + _SIZE - 1 >= Buffer.Size()) \
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
uint64_t input_base::Get_L8()
{
    TEST_BUFFEROVERFLOW(8);

    uint64_t ToReturn = Buffer[Buffer_Offset + 0] | (Buffer[Buffer_Offset + 1] << 8) | (Buffer[Buffer_Offset + 2] << 16) | (Buffer[Buffer_Offset + 3] << 24) | ((uint64_t)Buffer[Buffer_Offset + 4] << 32) | ((uint64_t)Buffer[Buffer_Offset + 5] << 40) | ((uint64_t)Buffer[Buffer_Offset + 6] << 48) | ((uint64_t)Buffer[Buffer_Offset + 7] << 56);
    Buffer_Offset += 8;
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
    if (!ToReturn)
        return (uint64_t)-1; // Out of specifications, consider the value as Unlimited
    uint64_t s = 0;
    while (!(ToReturn&(((uint64_t)1) << (7 - s))))
        s++;
    ToReturn ^= (((uint64_t)1) << (7 - s));
    TEST_BUFFEROVERFLOW(1 + s);
    uint64_t UnknownValue = (((uint64_t)1) << ((s + 1) * 7)) - 1;
    while (s)
    {
        ToReturn <<= 8;
        Buffer_Offset++;
        s--;
        ToReturn |= Buffer[Buffer_Offset];
    }
    if (ToReturn==UnknownValue)
        ToReturn=(uint64_t)-1; // Unknown size
    Buffer_Offset++;

    return ToReturn;
}

//---------------------------------------------------------------------------
void input_base::Error(error::type Type, error::generic::code Code)
{
    if (HasBufferOverflow())
        return; // Next errors are not real, due to buffer overflow
    if (!HasErrors())
    {
        switch (Type)
        {
            case error::type::Undecodable:
            case error::type::Unsupported:
                SetErrors();
                break;
            default:;
        }
    }
    if (Errors)
        Errors->Error(ParserCode, Type, Code);
}

//---------------------------------------------------------------------------
uncompressed::uncompressed(bool IsSequence_) :
    IsSequence(IsSequence_)
{
}

//---------------------------------------------------------------------------
uncompressed::~uncompressed()
{
}

//---------------------------------------------------------------------------
void unknown::ParseBuffer()
{
    SetDetected();
    RegisterAsAttachment();
}

//---------------------------------------------------------------------------
void input_base_uncompressed::RegisterAsAttachment()
{
    // Write RAWcooked file
    if (RAWcooked)
    {
        RAWcooked->Unique = true;
        RAWcooked->BeforeData = nullptr;
        RAWcooked->BeforeData_Size = 0;
        RAWcooked->AfterData = nullptr;
        RAWcooked->AfterData_Size = 0;
        RAWcooked->InData = nullptr;
        RAWcooked->InData_Size = 0;
        RAWcooked->FileSize = FileSize;
        if (RAWcooked->Version == rawcooked::version::v2 && FileSize)
        {
            RAWcooked->InData = Buffer.Data();
            RAWcooked->InData_Size = FileSize;
            RAWcooked->FileSize = (size_t)-1;
        }
        if (Actions[Action_Hash])
        {
            Hash();
            RAWcooked->HashValue = &HashValue;
        }
        else
            RAWcooked->HashValue = nullptr;
        RAWcooked->IsAttachment = true;
        RAWcooked->Parse();
    }
}

//---------------------------------------------------------------------------
file_output::~file_output()
{
    delete (MD5_CTX*)MD5;
}

