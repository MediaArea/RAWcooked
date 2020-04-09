/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Compressed/RAWcooked/Reversibility.h"
#include "Lib/Compressed/RAWcooked/Track.h"
#include <cstring>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace reversibility_issue {

    namespace undecodable
    {

        static const char* MessageText[] =
        {
            "Unreadable frame header",
            "More video frames in source content than saved frame headers",
        };

        namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

    } // unparsable

    const char** ErrorTexts[] =
    {
        undecodable::MessageText,
        nullptr,
        nullptr,
        nullptr,
    };

    static_assert(error::type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // reversibility_issue

using namespace reversibility_issue;

//---------------------------------------------------------------------------
// Compressed file can holds directory traversal filenames (e.g. ../../evil.sh)
// Not created by the encoder, but a malevolent person could craft such file
// https://snyk.io/research/zip-slip-vulnerability
void SanitizeFileName(buffer& FileName)
{
    auto FileName_Size = FileName.Size();
    auto FileName_Data = FileName.Data();

    // Replace illegal characters (on the target platform) by underscore
    // Note: the output is not exactly as the source content and information about the exact source file name is lost, this is a limitation of the target platform impossible to bypass
#if defined(_WIN32) || defined(_WINDOWS)
    for (size_t i = 0; i < FileName_Size; i++)
        if (FileName_Data[i] == ':'
            || (FileName_Data[i] == ' ' && ((i + 1 >= FileName_Size || FileName_Data[i + 1] == '.' || FileName_Data[i + 1] == PathSeparator) || (i == 0 || FileName_Data[i - 1] == PathSeparator)))
            || FileName_Data[i] == '<'
            || FileName_Data[i] == '>'
            || FileName_Data[i] == '|'
            || FileName_Data[i] == '\"'
            || FileName_Data[i] == '?'
            || FileName_Data[i] == '*')
            FileName_Data[i] = '_';
#endif

    // Trash leading path separator (used for absolute file names) ("///foo/bar" becomes "foo/bar")
    while (FileName_Size && FileName_Data[0] == PathSeparator)
    {
        FileName_Size--;
        memmove(FileName_Data, FileName_Data + 1, FileName_Size);
    }

    // Trash directory traversals ("../../foo../../ba..r/../.." becomes "foo../ba..r")
    for (size_t i = 0; FileName_Size > 1 && i < FileName_Size - 1; i++)
        if ((i == 0 || FileName_Data[i - 1] == PathSeparator) && FileName_Data[i] == '.' && FileName_Data[i + 1] == '.' && (i + 2 >= FileName_Size || FileName_Data[i + 2] == PathSeparator))
        {
            size_t Count = 2;
            if (i + 2 < FileName_Size)
                Count++;
            else if (i)
            {
                Count++;
                i--;
            }
            FileName_Size -= Count;
            memmove(FileName_Data + i, FileName_Data + i + Count, FileName_Size - i);
            i--;
        }
}

//---------------------------------------------------------------------------
void reversibility::NewFrame()
{
    Pos_ = Count_;
    Count_++;
}

//---------------------------------------------------------------------------
void reversibility::StartParsing()
{
    Pos_ = 0;
    if (!Count_ && Unique_)
        Count_++;
}

//---------------------------------------------------------------------------
void reversibility::NextFrame()
{
    Pos_++;
}

//---------------------------------------------------------------------------
bool reversibility::Unique() const
{
    return Unique_;
}

//---------------------------------------------------------------------------
void reversibility::SetUnique()
{
    if (Unique_)
        return;
    Unique_ = true;
}

//---------------------------------------------------------------------------
size_t reversibility::Pos() const
{
    return Pos_;
}

//---------------------------------------------------------------------------
size_t reversibility::Count() const
{
    return Count_;
}

//---------------------------------------------------------------------------
size_t reversibility::RemainingCount() const
{
    if (Pos_ >= Count_)
        return 0;
    return (Count_ - Pos_);
}

//---------------------------------------------------------------------------
size_t reversibility::ExtraCount() const
{
    if (Pos_ <= Count_)
        return 0;
    return (Pos_ - Count_);
}

//---------------------------------------------------------------------------
void reversibility::SetDataMask(element Element, buffer&& Buffer)
{
    Data_[(size_t)Element].SetDataMask(move(Buffer));
}

//---------------------------------------------------------------------------
void reversibility::SetData(element Element, buffer&& Buffer, bool AddMask)
{
    Data_[(size_t)Element].SetData(Pos_, move(Buffer), AddMask);

    if (Element == element::FileName)
        Data_[(size_t)element::FileName].SanitizeFileName(Pos_);
}

//---------------------------------------------------------------------------
buffer_view reversibility::Data(element Element) const
{
    return move(Data(Element, Pos_));
}

//---------------------------------------------------------------------------
buffer_view reversibility::Data(element Element, size_t Pos) const
{
    const auto ElementS = (size_t)Element;
    if (ElementS >= element_Max || Pos >= Count_)
        return buffer_view();
    const auto& Data = Data_[ElementS];
    return Data.Data(Pos);
}

//---------------------------------------------------------------------------
void reversibility::SetFileSize(uint64_t Value)
{
    FileSize_.SetData(Pos_, Value);
}

//---------------------------------------------------------------------------
uint64_t reversibility::GetFileSize() const
{
    return FileSize_.Data(Pos_);
}

//---------------------------------------------------------------------------
reversibility::data::~data()
{
    // Note about the memory model:
    // In order to speed up a bit the memory management, we avoid the buffer constructor
    // so we need to manually call the destructor here.
    // Check also SetData().
    for (size_t i = 0; i < MaxCount_; i++)
        Content_[i].Clear();
    delete[](uint8_t*)Content_;
}

//---------------------------------------------------------------------------
void reversibility::data::SetDataMask(buffer&& Buffer)
{
    Mask_ = move(Buffer);
}

//---------------------------------------------------------------------------
void reversibility::data::SetData(size_t Pos, buffer&& Buffer, bool AddMask)
{
    if (Pos >= MaxCount_)
    {
        // Note about the memory model:
        // In order to speed up a bit the memory management, we avoid the buffer constructor
        // and set all to 0 manually.
        // Check also the destructor.
        if (!MaxCount_ && !Pos)
        {
            MaxCount_ = 1;
            const auto NewSizeInBytes = sizeof(std::remove_pointer<decltype(Content_)>::type);
            Content_ = (decltype(Content_))new uint8_t[NewSizeInBytes];
            memset((uint8_t*)Content_, 0, NewSizeInBytes);
        }
        else
        {
            auto NewMaxCount = MaxCount_;
            if (!NewMaxCount)
                NewMaxCount = 1;
            while (Pos >= NewMaxCount)
                NewMaxCount *= 4;
            auto NewSizeInBytes = NewMaxCount * sizeof(std::remove_pointer<decltype(Content_)>::type);
            auto NewContent = (decltype(Content_))new uint8_t[NewSizeInBytes];
            if (Content_)
            {
                auto OldSizeInBytes = MaxCount_ * sizeof(std::remove_pointer<decltype(Content_)>::type);
                NewSizeInBytes -= OldSizeInBytes;
                memcpy((uint8_t*)NewContent, Content_, OldSizeInBytes);
                memset((uint8_t*)NewContent + OldSizeInBytes, 0, NewSizeInBytes);
                delete[] (uint8_t*)Content_;
            }
            Content_ = NewContent;
            MaxCount_ = NewMaxCount;
        }
    }

    Content_[Pos] = move(Buffer);

    if (AddMask)
    {
        auto Mask_Size = Mask_.Size();
        auto Mask_Data = Mask_.Data();
        if (!Mask_Size)
            return;

        auto Content_Size = Content_[Pos].Size();
        auto Content_Data = Content_[Pos].Data();
        for (size_t i = 0; i < Content_Size && i < Mask_Size; i++)
            Content_Data[i] += Mask_Data[i];
    }
}

//---------------------------------------------------------------------------
buffer_view reversibility::data::Data(size_t Pos) const
{
    if (Pos >= MaxCount_)
        return buffer_view();
    return buffer_view(Content_[Pos]);
}

//---------------------------------------------------------------------------
void reversibility::data::SanitizeFileName(size_t Pos)
{
    if (Pos >= MaxCount_)
        return;
    ::SanitizeFileName(Content_[Pos]);
}

//---------------------------------------------------------------------------
reversibility::filesize::~filesize()
{
    delete[] Content_;
}

//---------------------------------------------------------------------------
void reversibility::filesize::SetData(size_t Pos, uint64_t Value)
{
    if (Pos >= MaxCount_)
    {
        if (!MaxCount_ && !Pos)
        {
            MaxCount_ = 1;
            Content_ = new std::remove_pointer<decltype(Content_)>::type[1];
            *Content_ = (std::remove_pointer<decltype(Content_)>::type)0;
        }
        else
        {
            if (!MaxCount_)
                MaxCount_ = 1;
            while (Pos >= MaxCount_)
                MaxCount_ *= 4;
            auto NewContent = new std::remove_pointer<decltype(Content_)>::type[MaxCount_];
            if (Content_)
            {
                memcpy(NewContent, Content_, MaxCount_ * sizeof(std::remove_pointer<decltype(Content_)>::type));
                delete[] Content_;
            }
            Content_ = NewContent;
        }
    }

    Content_[Pos] = Value;
}

//---------------------------------------------------------------------------
uint64_t reversibility::filesize::Data(size_t Pos) const
{
    if (Pos >= MaxCount_)
        return 0;
    return Content_[Pos];
}
