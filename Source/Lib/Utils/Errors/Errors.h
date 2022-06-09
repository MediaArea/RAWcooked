/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

 //---------------------------------------------------------------------------
#ifndef ErrorsH
#define ErrorsH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Common/Common.h"
#include <bitset>
#include <cstdint>
#include <string>
#include <vector>
using namespace std;
class rawcooked;
class filemap;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Parser codes

enum parser
{
    Parser_DPX,
    Parser_TIFF,
    Parser_EXR,
    Parser_WAV,
    Parser_AIFF,
    Uncompressed_Max, // After this line, this isn't parsers of uncompressed data
    Parser_Matroska = Uncompressed_Max,
    Parser_ReversibilityData,
    Parser_HashSum,
    Parser_License,
    Parser_Unknown,
    Parser_Max, // After this line, parsers are not really parsers and must be specifically handled
    IO_IntermediateWriter = Parser_Max,
    IO_FileWriter,
    IO_FileChecker,
    IO_FileInput,
    IO_Hashes,
    IO_Max,
};

namespace error
{
    ENUM_BEGIN(type)
        Undecodable,
        Unsupported,
        Incoherent,
        Invalid,
    ENUM_END(type)

    namespace common
    {
        enum code : uint8_t;
        extern const char* MessageText[];
    }
    namespace undecodable
    {
        enum code : uint8_t;
    }
    namespace unsupported
    {
        enum code : uint8_t;
    }
    namespace incoherent
    {
        enum code : uint8_t;
    }
    namespace invalid
    {
        enum code : uint8_t;
    }
    namespace generic
    {
        enum code : uint8_t;
    }
    extern const char*** ErrorTexts[];
}

//---------------------------------------------------------------------------
// Helpers

#define IncoherencyMessage "Incoherency between enums and message strings"

//---------------------------------------------------------------------------
class errors
{
public:
    // Constructor / Destructor
    ~errors()                   { if (Parsers.size() > Parser_Max) DeleteStrings(); }
    
    // Error message read
    const char*                 ErrorMessage();

    // Error message write
    void                        Error(parser Parser, error::type Type, error::generic::code Code);
    void                        Error(parser Parser, error::type Type, error::generic::code Code, const string& String);
    bool                        HasErrors() { return HasErrors_Value; }
    bool                        HasWarnings() { return HasWarnings_Value; }
    void                        ClearErrors() { Parsers.clear(); }

private:
    struct per_parser
    {
        union info
        {
            size_t              Count;
            std::vector<string>*StringList;
        };
        std::vector<info>       Codes[error::type_Max];
    };
    std::vector<per_parser>     Parsers;
    string                      ErrorMessageCache;
    void                        DeleteStrings();
    bool                        HasErrors_Value = false;
    bool                        HasWarnings_Value = false;
};

#endif
