/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef BufferH
#define BufferH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Config.h"
#include <cstring>
using namespace std;
//---------------------------------------------------------------------------

struct buffer_base
{
public:
    const uint8_t* Data() const
    {
        return Data_;
    }

    const uint8_t& operator [] (size_t n) const
    {
        return Data_[n];
    }

    size_t Size() const
    {
        return Size_;
    }

    bool Empty() const
    {
        return !Size_;
    }

    operator string() const
    {
        return move(string((const char*)Data(), Size()));
    }

protected:
    buffer_base() = delete;
    buffer_base(const uint8_t* NewData, size_t NewSize) :
        Data_(NewData),
        Size_(NewSize)
    {}
    buffer_base(buffer_base& Buffer) = delete;
    buffer_base(buffer_base&& Buffer) = delete;
    buffer_base& operator = (const buffer_base& Buffer) = delete;
    buffer_base& operator = (const buffer_base&& Buffer) = delete;

    void ClearBase()
    {
        Size_ = 0;
        Data_ = nullptr;
    }

    void ClearKeepSizeBase() // Use it only as intermediate setting
    {
        Data_ = nullptr;
    }

    void AssignBase(const buffer_base& Buffer)
    {
        AssignBase(Buffer.Data(), Buffer.Size());
    }

    void AssignBase(const uint8_t* NewData, size_t NewSize)
    {
        Data_ = NewData;
        Size_ = NewSize;
    }

    void AssignKeepSizeBase(const uint8_t* NewData)
    {
        Data_ = NewData;
    }

    void AssignKeepDataBase(size_t NewSize) // Use it only as intermediate setting
    {
        Size_ = NewSize;
    }

private:
    const uint8_t* Data_;
    size_t Size_;
};

struct buffer : buffer_base
{
    buffer() :
        buffer_base(nullptr, 0)
    {}
    buffer(const buffer& Buffer) = delete;
    buffer(buffer&& Buffer) :
        buffer_base(Buffer.Data(), Buffer.Size())
    {
        Buffer.ClearBase();
    }

    buffer& operator = (buffer&& Buffer)
    {
        if (this == &Buffer)
            return *this;
        delete[] Data();
        AssignBase(Buffer);
        Buffer.ClearBase();
        return *this;
    }

    ~buffer()
    {
        delete[] Data();
    }

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
    uint8_t* Data() const
    {
        return (uint8_t*)buffer_base::Data(); // We are sure we can modify this buffer
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

    void Create(size_t NewSize)
    {
        delete[] Data();
        if (!NewSize)
        {
            ClearBase();
            return;
        }
        AssignBase(new uint8_t[NewSize], NewSize);
    }

    size_t CopyLimit(size_t Offset, const buffer_base& Buffer_Source)
    {
        auto SizeToCopy_Max = Size();
        if (Offset > SizeToCopy_Max)
            return 0;
        SizeToCopy_Max -= Offset;
        auto SizeToCopy = Buffer_Source.Size();
        if (SizeToCopy > SizeToCopy_Max)
            SizeToCopy = SizeToCopy_Max;
        memcpy(Data() + Offset, Buffer_Source.Data(), SizeToCopy);
        return SizeToCopy;
    }
    size_t CopyLimit(const buffer_base& Buffer_Source)
    {
        return CopyLimit(0, Buffer_Source);
    }

    void CopyExpand(const uint8_t* NewData, size_t NewSize)
    {
        Create(NewSize);
        memcpy(Data(), NewData, Size());
    }

    void SetZero()
    {
        memset(Data(), 0, Size());
    }

    size_t SetZero(size_t Offset, size_t Count)
    {
        auto SizeToZero = Size();
        if (Offset > SizeToZero)
            return 0;
        SizeToZero -= Offset;
        if (SizeToZero > Count)
            SizeToZero = Count;
        memset(Data() + Offset, 0, SizeToZero);
        return SizeToZero;
    }

    void Resize(size_t NewSize)
    {
        if (!NewSize)
        {
            ClearBase();
            return;
        }
        if (NewSize < Size())
        {
            AssignBase(Data(), NewSize); // We just change the Size value, no shrink of memory
            return;
        }
        auto NewBuffer = new uint8_t[NewSize];
        memcpy(NewBuffer, Data(), Size());
        AssignBase(NewBuffer, NewSize);
    }

    void Clear()
    {
        delete[] Data();
        ClearBase();
    }
};

struct buffer_view : buffer_base
{
    buffer_view() :
        buffer_base(nullptr, 0)
    {}
    buffer_view(const uint8_t* NewData, size_t NewSize) :
        buffer_base(NewData, NewSize)
    {}
    buffer_view(const buffer_view& Buffer) :
        buffer_base(Buffer.Data(), Buffer.Size())
    {}
    buffer_view(const buffer_base& Buffer) :
        buffer_base(Buffer.Data(), Buffer.Size())
    {}
    buffer_view(buffer_view&& Buffer) :
        buffer_base(Buffer.Data(), Buffer.Size())
    {
        Buffer.ClearBase();
    }

    buffer_view& operator = (const buffer_view& Buffer)
    {
        AssignBase(Buffer);
        return *this;
    }
    buffer_view& operator = (buffer_view&& Buffer)
    {
        if (this == &Buffer)
            return *this;
        AssignBase(Buffer);
        Buffer.ClearBase();
        return *this;
    }

    ~buffer_view() = default;
    
    void Clear()
    {
        ClearBase();
    }
};

struct buffer_or_view : buffer_base
{
    buffer_or_view() :
        buffer_base(nullptr, 0)
    {}
    buffer_or_view(const uint8_t* NewData, size_t NewSize) :
        buffer_base(NewData, NewSize)
    {}
    buffer_or_view(const buffer_or_view& Buffer) :
        buffer_base(Buffer.Data(), Buffer.Size())
    {}
    buffer_or_view(const buffer& Buffer) :
        buffer_base(Buffer.Data(), Buffer.Size())
    {}
    buffer_or_view(const buffer_view& Buffer) :
        buffer_base(Buffer.Data(), Buffer.Size())
    {}
    buffer_or_view(buffer_or_view&& Buffer) :
        buffer_base(Buffer.Data(), Buffer.Size()),
        IsOwned_(Buffer.IsOwned_)
    {
        Buffer.ClearBase();
        Buffer.IsOwned_ = false;
    }

    ~buffer_or_view()
    {
        if (!IsOwned_)
            return;
        delete[] Data();
    }

    buffer_or_view& operator = (buffer_or_view&& Buffer)
    {
        if (this == &Buffer)
            return *this;
        if (IsOwned_)
            Clear();
        AssignBase(Buffer);
        Buffer.ClearBase();
        if (Buffer.IsOwned_)
        {
            IsOwned_ = true;
            Buffer.IsOwned_ = false;
        }
        return *this;
    }

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
    uint8_t* DataForModification()
    {
        if (!IsOwned_ && Size())
        {
            auto NewBuffer = new uint8_t[Size()];
            memcpy(NewBuffer, Data(), Size());
            AssignKeepSizeBase(NewBuffer);
            IsOwned_ = true;
        }

        return (uint8_t*)Data(); // It is owned so modification is possible
    }
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

    void Create(size_t NewSize)
    {
        if (IsOwned_)
            delete[] Data();
        else
            IsOwned_ = true;
        AssignBase(new uint8_t[NewSize], NewSize);
    }

    void Assign(const uint8_t* NewData, size_t NewSize)
    {
        if (IsOwned_)
        {
            delete[] Data();
            IsOwned_ = false;
        }
        AssignBase(NewData, NewSize);
    }

    void CopyCut(const uint8_t* NewData, size_t NewSize)
    {
        Resize(NewSize);
        memcpy(DataForModification(), NewData, NewSize);
    }

    void Resize(size_t NewSize)
    {
        if (NewSize < Size())
        {
            AssignBase(Data(), NewSize); // We just change the Size value, no shrink of memory
            return;
        }
        auto OldBuffer = Data();
        auto NewBuffer = new uint8_t[NewSize];
        memcpy(NewBuffer, OldBuffer, NewSize);
        AssignBase(NewBuffer, NewSize);
        if (IsOwned_)
            delete[] OldBuffer;
        else
            IsOwned_ = true;
    }

    void Clear()
    {
        if (IsOwned_)
        {
            delete[] Data();
            IsOwned_ = false;
        }
        ClearBase();
    }

private:
    bool IsOwned_ = false;
};

//---------------------------------------------------------------------------
#endif
