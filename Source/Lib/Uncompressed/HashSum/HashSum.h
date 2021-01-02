/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef HashSumH
#define HashSumH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include <vector>
//---------------------------------------------------------------------------

class hashes
{
public:
    hashes(errors* Errors_Source = nullptr) :
        Errors(Errors_Source)
    {
    }

    // Options
    bool                        CheckFromFiles = false;
    bool                        WouldBeError = false;

    // Structs
    struct value
    {
        // Constructor / Destructor
        value() {}
        value(string const& Name_Source, md5 const& MD5_Source) : Name(Name_Source), MD5(MD5_Source) {}

        // Data
        string                  Name;
        md5                     MD5;
        enum flags
        {
            Flag_IsFound,
            Flag_IsDifferent,
            Flag_Max,
        };
        std::bitset<Flag_Max>   Flags;

        // Operators
        friend bool operator < (value const& l, value const& r) { return l.Name < r.Name || (l.Name == r.Name && l.MD5 < r.MD5); }
        friend bool operator == (value const& l, value const& r) { return l.Name == r.Name && l.MD5 == r.MD5; }
    };
    typedef std::vector<value>  list;

    // Actions
    size_t                      NewHashFile(); // Indicate that a new hash file is tentatively parsed
    void                        ResetHashFile(size_t OldSize); // Indicate that the new hash file is buggy, discard its content, use value from NewHashFile()
    void                        FromHashFile(string const& FileName, md5 const& MD5);
    void                        FromHashFile(buffer_base const& FileName, md5 const& MD5) { FromHashFile(string((const char*)FileName.Data(), FileName.Size()), MD5); }
    void                        Ignore(string const& FileName);
    void                        RemoveEmptyFiles();
    bool                        NoMoreHashFiles() { if (!IsSorted) NoMoreHashFiles_Internal(); return !List_FromHashFiles.empty(); } // Return true if Hashes are useful
    void                        FromFile(string const& FileName, md5 const& MD5);
    void                        Finish();

    // Info
    size_t                      HashFiles_Count() { return HashFiles.size(); }

private:
    // Internal
    void                        NoMoreHashFiles_Internal();

    // Data
    list                        List_FromHashFiles;
    list                        List_FromFiles;
    std::vector<string>         HashFiles;
    bool                        IsSorted = false;

    // Errors
    errors*                     Errors = nullptr;
};

class hashsum : public input_base_uncompressed
{
public:
    hashsum(errors* Errors = nullptr);

    string                      HomePath;

    // Data
    hashes*                     List = nullptr;

private:
    string                      Flavor_String() { return string(); } /// Not used
    void                        ParseBuffer();
    void                        BufferOverflow() {}; // Not used
};

//---------------------------------------------------------------------------
#endif
