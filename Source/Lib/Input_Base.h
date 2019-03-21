/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

 //---------------------------------------------------------------------------
#ifndef Input_BaseH
#define Input_BaseH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Errors.h"
#include <bitset>
#include <cstdint>
#include <string>
#include <vector>
using namespace std;
class rawcooked;
class filemap;
//---------------------------------------------------------------------------

enum action : uint8_t
{
    Action_Encode,
    Action_Max
};

enum info : uint8_t
{
    Info_IsDetected,
    Info_IsSupported,
    Info_HasErrors,
    Info_BufferOverflow,
    Info_Max
};

typedef uint8_t flavor;

//---------------------------------------------------------------------------
class input_base
{
public:
    // Constructor/Destructor
    input_base(parser ParserCode);
    input_base(errors* Errors, parser ParserCode);
    ~input_base();

    // Config
    bool                        AcceptTruncated = false;
    bool                        CheckPadding = false;
    bitset<Action_Max>          Actions = { 1 << Action_Encode };

    // Parse
    bool                        Parse(unsigned char* Buffer, size_t Buffer_Size);
    bool                        Parse(filemap& FileMap);

    // Info
    bool                        IsDetected() { return Info[Info_IsDetected]; }
    bool                        IsSupported() { return Info[Info_IsSupported]; }
    bool                        HasErrors() { return Info[Info_HasErrors]; }

    // Common info
    parser                      ParserCode;

protected:
    virtual void                ParseBuffer() = 0;
    virtual void                BufferOverflow() = 0;

    filemap*                    FileMap;
    unsigned char*              Buffer;
    size_t                      Buffer_Size;
    size_t                      Buffer_Offset;
    bool                        IsBigEndian;
    uint8_t                     Get_L1() { return Get_X1(); }
    uint8_t                     Get_B1() { return Get_X1(); }
    uint8_t                     Get_X1();
    uint16_t                    Get_L2();
    uint16_t                    Get_B2();
    uint32_t                    Get_X2() { return IsBigEndian ? Get_B2() : Get_L2(); }
    uint32_t                    Get_L4();
    uint32_t                    Get_B4();
    uint32_t                    Get_X4() { return IsBigEndian ? Get_B4() : Get_L4(); }
    double                      Get_XF4();
    uint64_t                    Get_B8();
    long double                 Get_BF10();
    uint64_t                    Get_EB();

    // Error message
    void                        Undecodable(error::undecodable::code Code) { Error(error::Undecodable, (error::generic::code)Code); }
    void                        Unsupported(error::unsupported::code Code) { Error(error::Unsupported, (error::generic::code)Code); }

    // Info
    void                        ClearInfo() { Info.reset(); }
    void                        SetDetected() { Info.set(Info_IsDetected); }
    void                        SetSupported() { Info.set(Info_IsSupported); }
    void                        SetErrors() { Info.set(Info_HasErrors); }
    void                        SetBufferOverflow() { if (HasBufferOverflow()) return; BufferOverflow(); Info.set(Info_BufferOverflow); }
    bool                        HasBufferOverflow() { return Info[Info_BufferOverflow]; }

private:
    // Errors
    void                        Error(error::type Type, error::generic::code Code);
    errors*                     Errors = NULL;

    // Info
    bitset<Info_Max>            Info;
};

class uncompressed
{
public:
    // Constructor/Destructor
    uncompressed(bool IsSequence = false);
    ~uncompressed();

    // Info
    flavor                      Flavor;
    virtual string              Flavor_String() = 0;

    // Common info
    bool                        IsSequence;
    rawcooked*                  RAWcooked;
};

class input_base_uncompressed : public input_base, public uncompressed
{
public:
    // Constructor/Destructor
    input_base_uncompressed(parser ParserCode, bool IsSequence = false) : input_base(ParserCode), uncompressed(IsSequence) {}
    input_base_uncompressed(errors* Errors, parser ParserCode, bool IsSequence = false) : input_base(Errors, ParserCode), uncompressed(IsSequence) {}
};

#endif
