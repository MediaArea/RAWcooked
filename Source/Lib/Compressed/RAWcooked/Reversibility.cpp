/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Compressed/RAWcooked/Reversibility.h"
#include "Lib/Compressed/RAWcooked/Track.h"
#include <cstring>
#include "zlib.h"
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
static void SanitizeFileName(buffer& FileName)
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
static bool Uncompress(const buffer_view& In, buffer& Out)
{
    // Size in EBML style
    auto Data = In.Data();
    auto Size = In.Size();
    if (!Size)
    {
        Out.Clear();
        return false;
    }
    decltype(Size) Offset = 0;
    uint64_t UncompressedSize = Data[0];
    if (!UncompressedSize)
        return true; // Out of specifications
    uint64_t s = 0;
    while (!(UncompressedSize & (((uint64_t)1) << (7 - s))))
        s++;
    UncompressedSize ^= (((uint64_t)1) << (7 - s));
    if (1 + s > Size)
        return true; // Problem
    while (s)
    {
        UncompressedSize <<= 8;
        Offset++;
        s--;
        UncompressedSize |= Data[Offset];
    }
    Offset++;

    // Read buffer
    if (UncompressedSize)
    {
        // Uncompressed
        Out.Create(UncompressedSize);

        uLongf t = (uLongf)UncompressedSize;
        if (uncompress((Bytef*)Out.Data(), &t, (const Bytef*)Data + Offset, (uLong)(Size - Offset)) < 0 || t != UncompressedSize)
            return true; // Problem
    }
    else
    {
        // Not compressed
        Out.Create(Data + Offset, Size - Offset);
    }

    return false;
}

//---------------------------------------------------------------------------
void reversibility::NewFrame()
{
    Pos_ = Count_;
    Count_++;
}

//---------------------------------------------------------------------------
void reversibility::StartParsing(const uint8_t* BaseData)
{
    Pos_ = 0;
    if (!Count_ && Unique_)
        Count_++;
    SetBaseData(BaseData);
}

//---------------------------------------------------------------------------
void reversibility::NextFrame()
{
    Pos_++;
}

//---------------------------------------------------------------------------
void reversibility::SetBaseData(const uint8_t* BaseData)
{
    BaseData_ = BaseData;
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
void reversibility::SetDataMask(element Element, const buffer_view& Buffer)
{
    Data_[(size_t)Element].SetDataMask(Buffer);
}

//---------------------------------------------------------------------------
void reversibility::SetData(element Element, size_t Offset, size_t Size, bool AddMask)
{
    Data_[(size_t)Element].SetData(Pos_, Offset, Size, AddMask);
}

//---------------------------------------------------------------------------
buffer reversibility::Data(element Element) const
{
    auto Content = Data(Element, Pos_);

    if (Element == element::FileName)
        SanitizeFileName(Content);

    return Content;
}

//---------------------------------------------------------------------------
buffer reversibility::Data(element Element, size_t Pos) const
{
    const auto ElementS = (size_t)Element;
    if (ElementS >= element_Max)
        return buffer();
    const auto& Data = Data_[ElementS];
    return Data.Data(BaseData_, Pos);
}

//---------------------------------------------------------------------------
void reversibility::SetFileSize(uint64_t Value)
{
    FileSize_.SetData(Pos_, Value);
}

//---------------------------------------------------------------------------
uint64_t reversibility::FileSize() const
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
    //for (size_t i = 0; i < MaxCount_; i++)
    //    Content_[i].Clear();
    delete[](uint8_t*)Content_;
}

//---------------------------------------------------------------------------
void reversibility::data::SetDataMask(const buffer_view& Buffer)
{
    Uncompress(Buffer, Mask_);
}

//---------------------------------------------------------------------------
void reversibility::data::SetData(size_t Pos, size_t Offset, size_t Size, bool AddMask)
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
            size_t OldSizeInBytes;
            if (Content_)
            {
                OldSizeInBytes = MaxCount_ * sizeof(std::remove_pointer<decltype(Content_)>::type);
                NewSizeInBytes -= OldSizeInBytes;
                memcpy((uint8_t*)NewContent, Content_, OldSizeInBytes);
                delete[](uint8_t*)Content_;
            }
            else
                OldSizeInBytes = 0;
            memset((uint8_t*)NewContent + OldSizeInBytes, 0, NewSizeInBytes);
            Content_ = NewContent;
            MaxCount_ = NewMaxCount;
        }
    }

    Content_[Pos].Offset = Offset;
    Content_[Pos].Size = Size;
    Content_[Pos].AddMask = AddMask;
}

//---------------------------------------------------------------------------
buffer reversibility::data::Data(const uint8_t* BaseData, size_t Pos) const
{
    // Uncompress
    buffer Content;
    if (Pos >= MaxCount_ || !BaseData || Uncompress(buffer_view(BaseData + Content_[Pos].Offset, Content_[Pos].Size), Content))
        return Content;

    // Add mask
    if (Content_[Pos].AddMask)
    {
        auto Mask_Size = Mask_.Size();
        auto Mask_Data = Mask_.Data();
        auto Content_Size = Content.Size();
        auto Content_Data = Content.Data();
        for (size_t i = 0; i < Content_Size && i < Mask_Size; i++)
            Content_Data[i] += Mask_Data[i];
    }

    return Content;
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
            auto OldMaxCount = MaxCount_;
            if (!MaxCount_)
                MaxCount_ = 1;
            while (Pos >= MaxCount_)
                MaxCount_ *= 4;
            auto NewContent = new std::remove_pointer<decltype(Content_)>::type[MaxCount_];
            if (OldMaxCount)
            {
                memcpy(NewContent, Content_, OldMaxCount * sizeof(std::remove_pointer<decltype(Content_)>::type));
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
