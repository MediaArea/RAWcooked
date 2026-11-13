/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Core/Metadata.h"
#include "Lib/Core/Input.h"
#include <cstdlib>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Order of the values of a field in 2 files, negative if Left is before Right, positive if Left is after Right, 0 if same
static int Compare(metadata_field::type Type, const string& Left, const string& Right)
{
    switch (Type)
    {
    case metadata_field::Unsigned:
    {
        const auto l = strtoull(Left.c_str(), nullptr, 10);
        const auto r = strtoull(Right.c_str(), nullptr, 10);
        return l < r ? -1 : l > r ? 1 : 0;
    }
    case metadata_field::Float: // Read with the numeric locale of the C library, as the parsers write the values
    {
        const auto l = strtod(Left.c_str(), nullptr);
        const auto r = strtod(Right.c_str(), nullptr);
        return l < r ? -1 : l > r ? 1 : 0;
    }
    default: // Text and time code: natural order, numbers inside the text are compared by value
        return input::FileName_Compare(Left, Right);
    }
}

//---------------------------------------------------------------------------
// True if 2 texts are the same once their numbers (runs of digits) are removed
static bool SameWithoutNumbers(const string& Left, const string& Right)
{
    const auto IsDigit = [](char Char) { return Char >= '0' && Char <= '9'; };
    size_t l = 0;
    size_t r = 0;
    for (;;)
    {
        while (l < Left.size() && IsDigit(Left[l]))
            l++;
        while (r < Right.size() && IsDigit(Right[r]))
            r++;
        if (l >= Left.size() || r >= Right.size())
            return l >= Left.size() && r >= Right.size();
        if (Left[l] != Right[r])
            return false;
        l++;
        r++;
    }
}

//---------------------------------------------------------------------------
// Kind of a field with a file more: Kind is the one for the files before it, Previous_Value the value in the last one of them, Value the value in the file
static void Kind_Add(metadata::kind& Kind, metadata_field::type Type, const string& Previous_Value, const string& Value)
{
    if (Kind == metadata::kind::Mixed || Value == Previous_Value)
        return;
    if (Value.empty() || Previous_Value.empty())
    {
        Kind = metadata::kind::Mixed; // Not set in some files only
        return;
    }
    if (Type == metadata_field::Text && !SameWithoutNumbers(Previous_Value, Value))
    {
        Kind = metadata::kind::Mixed; // Not an incremented value (e.g. 2 names in alphabetical order)
        return;
    }
    const auto Order = Compare(Type, Previous_Value, Value);
    if (Order < 0)
        Kind = metadata::kind::Sequence;
    else if (Order > 0)
        Kind = metadata::kind::Mixed; // Decreases
}

//---------------------------------------------------------------------------
void metadata::Init(vector<metadata_field>&& Fields)
{
    Fields_Value.clear();
    Fields_Value.resize(Fields.size());
    for (size_t i = 0; i < Fields.size(); i++)
        Fields_Value[i].Info = Fields[i];
    Ranges.clear();
    FileCount_Value = 0;
    IsComplete_Value = true;
}

//---------------------------------------------------------------------------
void metadata::Add(uint64_t Index, vector<string>&& Values, const vector<bool>& Missing)
{
    lock_guard<mutex> Lock(Mutex);

    // Ranges before and after the file
    auto Next = Ranges.upper_bound(Index);
    auto Previous = Next;
    if (Previous == Ranges.begin())
        Previous = Ranges.end(); // None
    else if ((--Previous)->second.End > Index)
        return; // The file at this position is already there
    Values.resize(Fields_Value.size()); // A value per field
    FileCount_Value++;

    // A field can be edited only if all files have a place for it
    for (size_t i = 0; i < Fields_Value.size() && i < Missing.size(); i++)
        if (Missing[i])
            Fields_Value[i].Info.Flags &= (uint8_t)~metadata_field::Editable;

    // The file extends the range it follows, else it is a new range
    if (Previous != Ranges.end() && Previous->second.End == Index)
    {
        auto& Range = Previous->second;
        for (size_t i = 0; i < Fields_Value.size(); i++)
            Kind_Add(Range.Kinds[i], Fields_Value[i].Info.Type, Range.Last[i], Values[i]);
        Range.Last.swap(Values); // The values of the previous file are freed by the caller, out of the lock
        Range.End++;
    }
    else
    {
        range Range;
        Range.End = Index + 1;
        Range.First = Values;
        Range.Last = move(Values);
        Range.Kinds.resize(Fields_Value.size(), kind::Same);
        Previous = Ranges.emplace_hint(Next, Index, move(Range));
    }

    // No more file missing before the next range
    if (Next != Ranges.end() && Next->first == Previous->second.End)
    {
        Merge(Previous->second, Next->second);
        Ranges.erase(Next);
    }
}

//---------------------------------------------------------------------------
void metadata::Finish()
{
    lock_guard<mutex> Lock(Mutex);

    // Files are missing before the first range or between ranges
    if (Ranges.size() > 1 || (!Ranges.empty() && Ranges.begin()->first))
        IsComplete_Value = false;
    while (Ranges.size() > 1)
    {
        auto Next = Ranges.begin();
        ++Next;
        Merge(Ranges.begin()->second, Next->second);
        Ranges.erase(Next);
    }
    if (Ranges.empty())
        return;

    // Summary of all files
    const auto& Range = Ranges.begin()->second;
    for (size_t i = 0; i < Fields_Value.size(); i++)
    {
        auto& Field = Fields_Value[i];
        Field.First = Range.First[i];
        Field.Last = Range.Last[i];
        Field.Kind = Range.Kinds[i];
    }
}

//---------------------------------------------------------------------------
void metadata::Merge(range& Range, range& Next)
{
    for (size_t i = 0; i < Fields_Value.size(); i++)
    {
        auto& Kind = Range.Kinds[i];
        if (Kind != kind::Mixed && Next.Kinds[i] != kind::Same)
            Kind = Next.Kinds[i]; // Not the same value in the files of the next range
        Kind_Add(Kind, Fields_Value[i].Info.Type, Range.Last[i], Next.First[i]);
    }
    Range.Last = move(Next.Last);
    Range.End = Next.End;
}
