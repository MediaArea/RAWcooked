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
#include "Lib/FileIO.h"
#include <bitset>
#include <cstdint>
#include <string>
#include <vector>
using namespace std;
class rawcooked;
class hashes;
//---------------------------------------------------------------------------

enum action : uint8_t
{
    Action_Encode,
    Action_Hash,
    Action_Coherency,
    Action_Conch,
    Action_CheckPadding,
    Action_AcceptGaps,
    Action_AcceptTruncated,
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
struct input_info
{
    double                      FrameRate = 0;
    size_t                      FrameCount = 0;
    double                      SampleRate = 0;
    size_t                      SampleCount = 0;
};

//---------------------------------------------------------------------------
class input_base
{
public:
    // Constructor/Destructor
    input_base(parser ParserCode);
    input_base(errors* Errors, parser ParserCode);
    virtual ~input_base();

    // Config
    bitset<Action_Max>          Actions;
    hashes*                     Hashes = nullptr;
    string*                     FileName = nullptr;

    // Parse
    bool                        Parse(uint8_t* Buffer, size_t Buffer_Size, size_t FileSize = (size_t)-1) { return Parse(nullptr, Buffer, Buffer_Size, FileSize); }
    bool                        Parse(filemap& FileMap) { return Parse(&FileMap, FileMap.Buffer, FileMap.Buffer_Size); }

    // Info
    bool                        IsDetected() { return Info[Info_IsDetected]; }
    bool                        IsSupported() { return Info[Info_IsSupported]; }
    bool                        HasErrors() { return Info[Info_HasErrors]; }
    input_info*                 InputInfo = nullptr;

    // Common info
    parser                      ParserCode;

protected:
    virtual void                ParseBuffer() = 0;
    virtual void                BufferOverflow() = 0;
    filemap*                    FileMap;
    size_t                      FileSize;
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
    void                        Unsupported(error::unsupported::code Code) { if (!Actions[Action_Encode]) return;  Error(error::Unsupported, (error::generic::code)Code); }
    void                        Invalid(error::invalid::code Code) { Error(error::Invalid, (error::generic::code)Code); }

    // Info
    void                        ClearInfo() { Info.reset(); }
    void                        SetDetected() { Info.set(Info_IsDetected); }
    void                        SetSupported() { Info.set(Info_IsSupported); }
    void                        SetErrors() { Info.set(Info_HasErrors); }
    void                        SetBufferOverflow() { if (HasBufferOverflow()) return; BufferOverflow(); Info.set(Info_BufferOverflow); }
    bool                        HasBufferOverflow() { return Info[Info_BufferOverflow]; }

protected:
    // Actions
    bool                        Parse(filemap* FileMap, uint8_t* Buffer, size_t Buffer_Size, size_t FileSize = (size_t)-1);
    void                        Hash();

    // Errors
    void                        Error(error::type Type, error::generic::code Code);
    errors*                     Errors = NULL;

    // Info
    bitset<Info_Max>            Info;
    bool                        HashComputed = false;
    md5                         HashValue;
};

class uncompressed
{
public:
    // Constructor/Destructor
    uncompressed(bool IsSequence = false);
    virtual ~uncompressed();

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
    virtual ~input_base_uncompressed() {}

protected:
    void                        RegisterAsAttachment();
};


class unknown : public input_base_uncompressed
{
public:
    unknown() : input_base_uncompressed(nullptr, Parser_Unknown, false) {}

private:
    string                      Flavor_String() { return string(); } /// Not used
    void                        ParseBuffer(); // Accept any
    void                        BufferOverflow() {} // Not used
};

#endif
