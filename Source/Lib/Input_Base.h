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
#include <cstdint>
#include <string>
using namespace std;
class rawcooked;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Parser codes
enum parser
{
    Parser_DPX,
    Parser_TIFF,
    Parser_WAV,
    Parser_AIFF,
    Parser_Max,
};

//---------------------------------------------------------------------------
class input_base
{
public:
    // Constructor/Destructor
    input_base();
    ~input_base();

    // Direct access to file map data
    unsigned char*              Buffer;
    size_t                      Buffer_Size;
    virtual bool                Parse(bool AcceptTruncated = false, bool FullCheck = false) = 0;

    // Error message
    const char*                 ErrorMessage();
    const char*                 ErrorType_Before();
    const char*                 ErrorType_After();

    // Info
    bool                        IsDetected;

protected:
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
    bool                        Unsupported(const char* Message);
    bool                        Invalid(const char* Message);

private:
    const char*                 Error_Message;
    enum error_type
    {
        Error_Unsupported,
        Error_Invalid,
    };
    error_type                  Error_Type;
};


class uncompressed
{
public:
    // Constructor/Destructor
    uncompressed(parser ParserCode, bool IsSequence = false);
    ~uncompressed();

    // Info
    uint8_t                     Flavor;
    virtual string              Flavor_String() = 0;

    // Common info
    bool                        IsSequence;
    rawcooked*                  RAWcooked;
    parser                      ParserCode;
};

class input_base_uncompressed : public input_base, public uncompressed
{
public:
    // Constructor/Destructor
    input_base_uncompressed(parser ParserCode, bool IsSequence = false) : input_base(), uncompressed(ParserCode, IsSequence) {}
};

#endif
