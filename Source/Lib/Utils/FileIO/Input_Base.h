/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

 //---------------------------------------------------------------------------
#ifndef Input_BaseH
#define Input_BaseH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
#include "Lib/Utils/Errors/Errors.h"
#include "Lib/Utils/FileIO/FileIO.h"
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
    Action_Decode,
    Action_Hash,
    Action_Coherency,
    Action_Conch,
    Action_CheckPadding,
    Action_CheckPaddingOptionIsSet,
    Action_AcceptGaps,
    Action_VersionValueIsAuto,
    Action_Version2,
    Action_AcceptTruncated,
    Action_Check,
    Action_CheckOptionIsSet,
    Action_Info,
    Action_FrameMd5,
    Action_FrameMd5An,
    Action_QuickCheckAfterEncode, // Internal, indicating the 2nd pass
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
    const string*               FileName = nullptr;
    const string*               OpenName = nullptr; // TODO: merge with FileName
    filemap::method             OpenStyle = {};

    // Parse
    bool                        Parse(const buffer_view& Buffer, size_t FileSize = (size_t)-1) { return Parse(nullptr, Buffer, FileSize); }
    bool                        Parse(filemap& FileMap) { return Parse(&FileMap, FileMap); }

    // Info
    bool                        IsDetected() { return Info[Info_IsDetected]; }
    bool                        IsSupported() { return Info[Info_IsSupported] && !HasErrors(); }
    bool                        HasErrors() { return Info[Info_HasErrors]; }
    input_info*                 InputInfo = nullptr;

    // Common info
    parser                      ParserCode;

protected:
    virtual void                ParseBuffer() = 0;
    virtual void                BufferOverflow() = 0;
    filemap*                    FileMap;
    filemap*                    FileMap2;
    size_t                      FileSize;
    buffer_view                 Buffer;
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
    uint64_t                    Get_L8();
    uint64_t                    Get_B8();
    long double                 Get_BF10();
    uint64_t                    Get_EB();

    // Error message
    void                        Undecodable(error::undecodable::code Code) { Error(error::type::Undecodable, (error::generic::code)Code); }
    void                        Unsupported(error::unsupported::code Code) { if (!Actions[Action_Encode]) return;  Error(error::type::Unsupported, (error::generic::code)Code); }
    void                        Invalid(error::invalid::code Code) { Error(error::type::Invalid, (error::generic::code)Code); }

    // Info
    void                        ClearInfo() { Info.reset(); }
    void                        SetDetected() { Info.set(Info_IsDetected); }
    void                        SetSupported() { Info.set(Info_IsSupported); }
    void                        SetErrors() { Info.set(Info_HasErrors); }
    void                        SetBufferOverflow() { if (HasBufferOverflow()) return; BufferOverflow(); Info.set(Info_BufferOverflow); }
    bool                        HasBufferOverflow() { return Info[Info_BufferOverflow]; }

protected:
    // Actions
    bool                        Parse(filemap* FileMap, const buffer_view& Buffer, size_t FileSize = (size_t)-1);
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
    uint64_t                    Flavor = (uint64_t)-1;
    virtual string              Flavor_String() = 0;

    // Common info
    bool                        IsSequence;
    rawcooked*                  RAWcooked = nullptr;

    // Features
    rawcooked::version          Version() { return RAWcooked ? RAWcooked->Version : rawcooked::version::v1; }
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

class input_base_uncompressed_video : public input_base_uncompressed
{
public:
    // Constructor/Destructor
    input_base_uncompressed_video(parser ParserCode) : input_base_uncompressed(ParserCode) {}
    input_base_uncompressed_video(errors* Errors, parser ParserCode, bool IsSequence = false) : input_base_uncompressed(Errors, ParserCode, IsSequence) {}
    virtual ~input_base_uncompressed_video() {}
};

class input_base_uncompressed_audio : public input_base_uncompressed
{
public:
    // Constructor/Destructor
    input_base_uncompressed_audio(parser ParserCode) : input_base_uncompressed(ParserCode) {}
    input_base_uncompressed_audio(errors* Errors, parser ParserCode, bool IsSequence = false) : input_base_uncompressed(Errors, ParserCode, IsSequence) {}
    virtual ~input_base_uncompressed_audio() {}

    // Info about formats
    virtual uint8_t             BitDepth() = 0;
    virtual sign                Sign() = 0;
    virtual endianness          Endianness() = 0;
};

struct file_output
{
    ~file_output();

    file                        Write;
    filemap                     Read;
    size_t                      Offset = 0;
    void*                       MD5 = nullptr; // MD5_CTX*
};

class input_base_uncompressed_compound : public input_base_uncompressed_audio
{
public:
    // Constructor/Destructor
    input_base_uncompressed_compound(parser ParserCode) : input_base_uncompressed_audio(ParserCode) {}
    input_base_uncompressed_compound(errors* Errors, parser ParserCode, bool IsSequence = false) : input_base_uncompressed_audio(Errors, ParserCode, IsSequence) {}

    // Info about formats
    virtual size_t              GetStreamCount() = 0;

    // Demux
    uint64_t                    InputOutput_Diff = 0;
    string                      Output_FileName;
    struct position
    {
        uint16_t                Index;
        uint32_t                Size;
        uint64_t                Input_Offset;
        uint64_t                Output_Offset;
        buffer*                 Buffer;

        ~position()
        {
            delete Buffer;
        }
    };
    vector<position>            Positions;
    size_t                      Positions_Offset_InFileWritten = 0;
    size_t                      Positions_Offset_Video = 0;
    size_t                      Positions_Offset_Audio = 0;
    size_t                      Positions_Offset_Audio_AdditionalBytes = 0;
    buffer                      Input;
    file_output                 Output;
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
