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
    Parser_WAV,
    Parser_AIFF,
    Parser_Matroska,
    Parser_License,
    Parser_Max, // After this line, parsers are not really parsers and must be specifically handled
    IO_IntermediateWriter = Parser_Max,
    IO_FileWriter,
    IO_FileChecker,
    IO_Max,
};

namespace error
{
    enum type : uint8_t
    {
        Undecodable,
        Unsupported,
        Type_Max
    };

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
    void                        ClearErrors() { Parsers.clear(); }

private:
    struct per_parser
    {
        union info
        {
            size_t              Count;
            std::vector<string>*StringList;
        };
        std::vector<info>       Codes[error::Type_Max];
    };
    std::vector<per_parser>     Parsers;
    string                      ErrorMessageCache;
    void                        DeleteStrings();
    bool                        HasErrors_Value = false;
};

#endif
