/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef MetadataH
#define MetadataH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Utils/FileIO/Input_Base.h"
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Summary of the metadata of the files of a track: for each field, the value if it is the same in all files,
// else the values of the first and last files and if the values never decrease along the files (e.g. frame number, date)
class metadata
{
public:
    enum class kind : uint8_t
    {
        Same,       // Same value in all files
        Sequence,   // Values never decrease from a file to the next one, and are not all the same; for a text, only the numbers inside it change
        Mixed,      // Other
    };
    struct field
    {
        metadata_field          Info;       // Not editable if a file has no place for the field
        string                  First;      // Value in the first file, empty if not set
        string                  Last;       // Value in the last file
        kind                    Kind = kind::Same;
    };

    // Fill
    void                        Init(vector<metadata_field>&& Fields);
    void                        Add(uint64_t Index, vector<string>&& Values, const vector<bool>& Missing = vector<bool>()); // Values of the file at this position in the track, from any thread and in any order; Missing: for each field, if the file has no place for it
    void                        Finish(); // No more file

    // Result, after Finish()
    const vector<field>&        Fields() const { return Fields_Value; }
    size_t                      FileCount() const { return FileCount_Value; } // Count of files having provided values
    bool                        IsComplete() const { return IsComplete_Value; } // False if values of some files are missing (e.g. a file not parsed after an error); files missing after the last one having provided values are not known here, see FileCount()

private:
    // Summary of files following each other in the track
    struct range
    {
        uint64_t                End = 0;    // Position in the track after the last file
        vector<string>          First;      // Values of the first file
        vector<string>          Last;       // Values of the last file
        vector<kind>            Kinds;
    };
    void                        Merge(range& Range, range& Next); // Next, after Range in the track, is added to Range

    vector<field>               Fields_Value;
    map<uint64_t, range>        Ranges; // By position of their first file. Files arrive in any order, they are compared in the order of the track: a range per group of files following each other, so their count depends on the count of threads providing files, not on the count of files
    size_t                      FileCount_Value = 0;
    bool                        IsComplete_Value = true;
    mutex                       Mutex;
};

#endif
