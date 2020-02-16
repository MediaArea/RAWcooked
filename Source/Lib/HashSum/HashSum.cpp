/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/HashSum/HashSum.h"
#include <algorithm>
using namespace std;
//---------------------------------------------------------------------------

namespace hashes_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "files from output are not same as files from source",
    "files present but not listed in source",
    "file names listed in source but not present",
};

enum code : uint8_t
{
    FileComparison,
    FileMissing,
    FileExtra,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // undecodable

namespace invalid
{

static const char* MessageText[] =
{
    "Computed hashes are not same as hashes in hash files",
    "files present but not listed in hash file",
    "file names listed in hash file but not present",
};

enum code : uint8_t
{
    FileHashComparison,
    FileHashMissing,
    FileHashExtra,
    Max
};

namespace invalid { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unparsable

const char** ErrorTexts[] =
{
    undecodable::MessageText,
    nullptr,
    nullptr,
    invalid::MessageText,
};

static_assert(error::Type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // filechecker_issue

//***************************************************************************
// Hashes
//***************************************************************************

//---------------------------------------------------------------------------
struct Hash_FileSearch
{
    bool operator() (hashes::value const& v, string const& s) { return v.Name < s; }
    bool operator() (string const& s, hashes::value const& v) { return s < v.Name; }
};

//---------------------------------------------------------------------------
size_t hashes::NewHashFile()
{
    return List_FromHashFiles.size();
}

//---------------------------------------------------------------------------
void hashes::ResetHashFile(size_t OldSize)
{
    List_FromHashFiles.resize(OldSize);
}

//---------------------------------------------------------------------------
void hashes::FromHashFile(string const& FileName, md5 const& MD5)
{
    List_FromHashFiles.emplace_back(FileName, MD5);
}

//---------------------------------------------------------------------------
void hashes::Ignore(string const& FileName)
{
    HashFiles.push_back(FileName);
}

//---------------------------------------------------------------------------
void hashes::RemoveEmptyFiles()
{
    md5 EmptyMD5 = { 0xd4, 0x1d, 0x8c, 0xd9, 0x8f, 0x00, 0xb2, 0x04, 0xe9, 0x80, 0x09, 0x98, 0xec, 0xf8, 0x42, 0x7e };

    auto List_FromHashFiles_Size = List_FromHashFiles.size();
    for (size_t i = 0; i < List_FromHashFiles_Size; i++)
    {
        if (List_FromHashFiles[i].MD5 == EmptyMD5)
        {
            List_FromHashFiles.erase(List_FromHashFiles.begin() + i);
            List_FromHashFiles_Size--;
        }
    }
}

//---------------------------------------------------------------------------
void hashes::FromFile(string const& FileName, md5 const& MD5)
{
    // Hash files maybe not yet there, we wait if we don't know that files are not all there
    if (!IsSorted)
    {
        if (find(HashFiles.begin(), HashFiles.end(), FileName) == HashFiles.end())
            List_FromFiles.emplace_back(FileName, MD5);
        return;
    }

    // Coherency
    if (!CheckFromFiles && List_FromHashFiles.empty())
        return;

    auto Files = equal_range(CheckFromFiles ? List_FromFiles.begin() : List_FromHashFiles.begin(), CheckFromFiles ? List_FromFiles.end() : List_FromHashFiles.end(), FileName, Hash_FileSearch());
    if (Files.first == Files.second)
    {
        if (find(HashFiles.begin(), HashFiles.end(), FileName) == HashFiles.end())
        {
            if (Errors)
                Errors->Error(IO_Hashes, CheckFromFiles ? error::Undecodable : error::Invalid, CheckFromFiles ? (error::generic::code)hashes_issue::undecodable::FileMissing : (error::generic::code)hashes_issue::invalid::FileHashMissing, FileName);
        }
    }
    else
    {
        for (auto File = Files.first; File != Files.second; ++File)
        {
            File->Flags.set(hashes::value::Flag_IsFound);
            if (File->MD5 != MD5)
            {
                if (Errors)
                    Errors->Error(IO_Hashes, CheckFromFiles ? error::Undecodable : error::Invalid, CheckFromFiles ? (error::generic::code)hashes_issue::undecodable::FileComparison : (error::generic::code)hashes_issue::invalid::FileHashComparison, FileName);
            }
        }
    }
}

//---------------------------------------------------------------------------
void hashes::NoMoreHashFiles_Internal()
{
    // Coherency
    if (IsSorted || (!CheckFromFiles && List_FromHashFiles.empty()))
        return;
    IsSorted = true;

    // Sort and make unique
    sort(List_FromHashFiles.begin(),List_FromHashFiles.end());
    List_FromHashFiles.erase(unique(List_FromHashFiles.begin(), List_FromHashFiles.end()), List_FromHashFiles.end());

    // If hash files are present, parse already registered files
    if (!List_FromHashFiles.empty())
    {
        for (auto Value : List_FromFiles)
            FromFile(Value.Name, Value.MD5);
    }
}

//---------------------------------------------------------------------------
void hashes::Finish()
{
    // Coherency
    if (!IsSorted || (!CheckFromFiles && List_FromHashFiles.empty()))
        return;

    for (auto HashItem : List_FromHashFiles)
    {
        if (!HashItem.Flags[hashes::value::Flag_IsFound])
        {
            if (Errors)
                Errors->Error(IO_Hashes, CheckFromFiles ? error::Undecodable : error::Invalid, CheckFromFiles ? (error::generic::code)hashes_issue::undecodable::FileExtra : (error::generic::code)hashes_issue::invalid::FileHashExtra, HashItem.Name);
        }
    }
}

//***************************************************************************
// HashSum
//***************************************************************************

//---------------------------------------------------------------------------
hashsum::hashsum(errors* Errors_Source) :
    input_base_uncompressed(Errors_Source, Parser_HashSum, true)
{
}

//---------------------------------------------------------------------------
void hashsum::ParseBuffer()
{
    size_t OldSize;
    if (List)
        OldSize = List->NewHashFile();
    Buffer_Offset = 0;

    // Handle path and name, remove file name
    FormatPath(HomePath);
    size_t PathSeparatorPos = HomePath.rfind('/');
    if (PathSeparatorPos != (size_t)-1 && PathSeparatorPos + 1 != HomePath.size())
        HomePath.resize(PathSeparatorPos + 1);

    while (Buffer_Offset + 32 + 1 <= Buffer_Size)
    {
        string Name;
        md5 MD5;

        // Check BSD format (also used on macOS)
        bool IsBsd;
        array<uint8_t, 5> BsdStringBegin_Md5 = { 'M', 'D', '5', ' ', '(' };
        array<uint8_t, 4> BsdStringEnd = { ')', ' ', '=', ' ' };
        if (!memcmp(Buffer+Buffer_Offset, BsdStringBegin_Md5.data(), BsdStringBegin_Md5.size()))
        {
            // Name
            Buffer_Offset += BsdStringBegin_Md5.size();
            size_t Begin = Buffer_Offset;
            while (Buffer_Offset < Buffer_Size && Buffer[Buffer_Offset] != '\r' && Buffer[Buffer_Offset] != '\n')
                Buffer_Offset++;
            if (Buffer_Offset - Begin < BsdStringEnd.size() + 32)
                break; // Not enough place for ") = " + MD5
            Buffer_Offset -= 32;
            if (memcmp(Buffer + Buffer_Offset - BsdStringEnd.size(), BsdStringEnd.data(), BsdStringEnd.size()))
                break;
            Name.assign((const char*)Buffer + Begin, Buffer_Offset - BsdStringEnd.size() - Begin);
            IsBsd = true;
        }
        else
            IsBsd = false;

        // MD5
        for (uint8_t i = 0; i < 16; i++)
        {
            char c1 = Buffer[Buffer_Offset++];
            char c2 = Buffer[Buffer_Offset++];
            if (c1 >= '0' && c1 <= '9')
                c1 -= '0';
            else if (c1 >= 'a' && c1 <= 'f')
            {
                c1 -= 'a';
                c1 += 10;
            }
            else if (c1 >= 'A' && c1 <= 'F')
            {
                c1 -= 'A';
                c1 += 10;
            }
            else
                break;
            if (c2 >= '0' && c2 <= '9')
                c2 -= '0';
            else if (c2 >= 'a' && c2 <= 'z')
            {
                c2 -= 'a';
                c2 += 10;
            }
            else if (c2 >= 'A' && c2 <= 'Z')
            {
                c2 -= 'A';
                c2 += 10;
            }
            else
                break;
            MD5[i] = (c1 << 4) | (c2);
        }

        if (!IsBsd)
        {
            // Separator(s)
            if (Buffer_Offset >= Buffer_Size)
                break;
            if (Buffer[Buffer_Offset++] != ' ')
                break;
            if (Buffer_Offset >= Buffer_Size)
                break;
            uint8_t c = Buffer[Buffer_Offset];
            if (c == ' ' || c == '*' || c == '?') // "?" is found in shasum --portable
                Buffer_Offset++;

            // Name
            size_t Begin = Buffer_Offset;
            while (Buffer_Offset < Buffer_Size && Buffer[Buffer_Offset] != '\r' && Buffer[Buffer_Offset] != '\n')
                Buffer_Offset++;
            Name.assign((const char*)Buffer + Begin, Buffer_Offset - Begin);
        }
        
        // Add
        if (List)
            List->FromHashFile(HomePath + Name, MD5);

        // End of line
        if (Buffer[Buffer_Offset++] == '\r')
        {
            if (Buffer_Offset < Buffer_Size && Buffer[Buffer_Offset++] != '\n')
                break;
        }
    }

    // Integrity test
    if (Buffer_Offset != Buffer_Size)
    {
        if (List)
            List->ResetHashFile(OldSize);
        return;
    }

    // Detected
    SetDetected();
    SetSupported();
    RegisterAsAttachment();
}
