/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Utils/FileIO/FileIO.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include <cmath>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
namespace dpx_issue { extern const char** ErrorTexts[]; }
namespace tiff_issue { extern const char** ErrorTexts[]; }
namespace exr_issue { extern const char** ErrorTexts[]; }
namespace wav_issue { extern const char** ErrorTexts[]; }
namespace aiff_issue { extern const char** ErrorTexts[]; }
namespace matroska_issue { extern const char** ErrorTexts[]; }
namespace reversibility_issue { extern const char** ErrorTexts[]; }
namespace intermediatewrite_issue { extern const char** ErrorTexts[]; }
namespace filewriter_issue { extern const char** ErrorTexts[]; }
namespace filechecker_issue { extern const char** ErrorTexts[]; }
namespace fileinput_issue { extern const char** ErrorTexts[]; }
namespace hashes_issue { extern const char** ErrorTexts[]; }
static const char*** AllErrorTexts[] =
{
    dpx_issue::ErrorTexts,
    tiff_issue::ErrorTexts,
    exr_issue::ErrorTexts,
    wav_issue::ErrorTexts,
    aiff_issue::ErrorTexts,
    matroska_issue::ErrorTexts,
    reversibility_issue::ErrorTexts,
    nullptr,
    nullptr,
    nullptr,
    intermediatewrite_issue::ErrorTexts,
    filewriter_issue::ErrorTexts,
    filechecker_issue::ErrorTexts,
    fileinput_issue::ErrorTexts,
    hashes_issue::ErrorTexts,
};
static_assert(IO_Max == sizeof(AllErrorTexts) / sizeof(const char***), IncoherencyMessage); \


//---------------------------------------------------------------------------
static const char* ParserNames[] =
{
    "DPX",
    "TIFF",
    "EXR",
    "WAV",
    "AIFF",
    "Matroska",
    "Reversibility data",
    "HashSum",
    "RAWcooked license",
    "Unknown",
};
static_assert(Parser_Max == sizeof(ParserNames) / sizeof(const char*), IncoherencyMessage); \

//---------------------------------------------------------------------------
static const char* ErrorTypes_Strings[] =
{
    "Error: undecodable",
    "Error: unsupported",
    "Warning: incoherent",
    "Warning: non-conforming",
};
static_assert(error::type_Max == sizeof(ErrorTypes_Strings) / sizeof(const char*), IncoherencyMessage); \

//---------------------------------------------------------------------------
static const char* ErrorTypes_Infos[] =
{
    nullptr,
    "Please contact info@mediaarea.net if you want support of such content",
    "Incoherency in characteristics of files is detected",
    "At least 1 file is not conform to specifications",
};
static_assert(error::type_Max == sizeof(ErrorTypes_Infos) / sizeof(const char*), IncoherencyMessage); \

//---------------------------------------------------------------------------
const char* errors::ErrorMessage()
{
    if (Parsers.empty())
        return NULL;

    ErrorMessageCache = '\n';

    bitset<error::type_Max> HasErrors;

    for (uint8_t i = 0; i < Parsers.size(); i++)
        for (uint8_t j = 0; j < error::type_Max; j++)
        {
            for (uint8_t k = 0; k < Parsers[i].Codes[j].size(); k++)
                if (i < Parser_Max)
                {
                    if (Parsers[i].Codes[j][k].Count)
                    {
                        HasErrors.set(j);
                        ErrorMessageCache += ErrorTypes_Strings[j];
                        ErrorMessageCache += ' ';
                        ErrorMessageCache += ParserNames[i];
                        ErrorMessageCache += ' ';
                        if (AllErrorTexts[i] && AllErrorTexts[i][j] && AllErrorTexts[i][j][k])
                            ErrorMessageCache += AllErrorTexts[i][j][k];
                        else
                            ErrorMessageCache += to_string(k);
                        if (Parsers[i].Codes[j][k].Count > 1)
                        {
                            ErrorMessageCache += " (x";
                            ErrorMessageCache += to_string(Parsers[i].Codes[j][k].Count);
                            ErrorMessageCache += ')';
                        }
                        ErrorMessageCache += '.';
                        ErrorMessageCache += '\n';
                    }
                }
                else if (i < IO_Max)
                {
                    if (Parsers[i].Codes[j][k].StringList)
                    {
                        ErrorMessageCache += ErrorTypes_Strings[j];
                        ErrorMessageCache += ' ';
                        ErrorMessageCache += AllErrorTexts[i][j][k];
                        ErrorMessageCache += '.';
                        ErrorMessageCache += '\n';
                        std::vector<string>& List = *Parsers[i].Codes[j][k].StringList;
                        for (size_t l = 0; l < List.size(); l++)
                        {
                            ErrorMessageCache += "       ";
                            ErrorMessageCache += List[l];
                            ErrorMessageCache += '\n';
                        }
                    }
                }
        }
    for (uint8_t j = 0; j < error::type_Max; j++)
        if (ErrorTypes_Infos[j] && HasErrors[j])
        {
            ErrorMessageCache += ErrorTypes_Infos[j];
            ErrorMessageCache += '.';
            ErrorMessageCache += '\n';
        }

    return ErrorMessageCache.c_str();
}

//---------------------------------------------------------------------------
void errors::Error(parser Parser, error::type Type, error::generic::code Code)
{
    if (Parser >= Parsers.size())
        Parsers.resize(Parser + 1);
    std::vector<per_parser::info> & Codes = Parsers[Parser].Codes[(size_t)Type];
    if (Code >= Codes.size())
        Codes.resize(Code + 1);

    Codes[Code].Count++;
    switch (Type)
    {
        case error::type::Undecodable:
        case error::type::Unsupported:
            if (!HasErrors_Value)
                HasErrors_Value = true;
            break;
        case error::type::Incoherent:
        case error::type::Invalid:
            if (!HasWarnings_Value)
                HasWarnings_Value = true;
            break;
    }
}

//---------------------------------------------------------------------------
void errors::Error(parser Parser, error::type Type, error::generic::code Code, const string& String)
{
    if (Parser >= Parsers.size())
        Parsers.resize(Parser + 1);
    std::vector<per_parser::info> & Codes = Parsers[Parser].Codes[(size_t)Type];
    if (Code >= Codes.size())
        Codes.resize(Code + 1);

    if (!Codes[Code].StringList)
        Codes[Code].StringList = new std::vector<string>;
    std::vector<string>& List = *Codes[Code].StringList;
    if (List.size() >= 11)
        return;
    if (List.size() >= 10)
    {
        List.push_back("...");
        return;
    }
    if (!List.empty() && List.back() == String)
        return;
    List.push_back(String);
    if ((Type == error::type::Undecodable || Type == error::type::Unsupported) && !HasErrors_Value)
        HasErrors_Value = true;

    switch (Type)
    {
        case error::type::Undecodable:
        case error::type::Unsupported:
            if (!HasErrors_Value)
                HasErrors_Value = true;
            break;
        case error::type::Incoherent:
        case error::type::Invalid:
            if (!HasWarnings_Value)
                HasWarnings_Value = true;
            break;
    }
}

//---------------------------------------------------------------------------
void errors::DeleteStrings()
{
    for (uint8_t i = Parser_Max + 1; i < Parsers.size(); i++)
        for (uint8_t j = 0; j < error::type_Max; j++)
            for (uint8_t k = 0; k < Parsers[i].Codes[j].size(); k++)
                delete Parsers[i].Codes[j][k].StringList;
}

