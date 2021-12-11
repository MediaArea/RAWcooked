/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef ReversibilityDataH
#define ReversibilityDataH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Utils/Buffer/Buffer.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
class reversibility
{
public:
    // Enums
    ENUM_BEGIN(element)
        FileName,
        BeforeData,
        AfterData,
        InData,
    ENUM_END(element)

    // Actions - Storing
    void                        SetBaseData(const uint8_t* BaseData);
    void                        SetUnique();
    void                        NewFrame();
    void                        SetDataMask(element Element, const buffer_view& Buffer);
    void                        SetData(element Element, size_t Offset, size_t Size, bool AddMask);
    void                        SetFileSize(uint64_t Value);

    // Actions - Parsing
    void                        StartParsing(const uint8_t* BaseData);
    void                        NextFrame();

    // Data
    buffer                      Data(element Element) const;
    buffer                      Data(element Element, size_t Pos) const;

    // Info
    bool                        Unique() const;
    size_t                      Pos() const;
    size_t                      Count() const;
    size_t                      RemainingCount() const;
    size_t                      ExtraCount() const;
    uint64_t                    FileSize() const;

private:
    struct data
    {
        ~data();

        // Set
        void                    SetDataMask(const buffer_view& Buffer);
        void                    SetData(size_t Pos, size_t Offset, size_t Size, bool AddMask);

        // Get
        buffer                  Data(const uint8_t* BaseData, size_t Pos) const;

    private:
        buffer                  Mask_;
        struct offset_size
        {
            size_t              Offset;
            size_t              Size;
            bool                AddMask;
        };
        offset_size*            Content_;
        size_t                  MaxCount_ = 0;
    };
    data                        Data_[element_Max];

    struct filesize
    {
        ~filesize();

        // Set
        void                    SetData(size_t Pos, uint64_t Value);

        // Get
        uint64_t                Data(size_t Pos) const;

    private:
        uint64_t*               Content_ = nullptr;
        size_t                  MaxCount_ = 0;
    };
    filesize                    FileSize_;

    size_t                      Pos_ = 0;
    size_t                      Count_ = 0;
    const uint8_t*              BaseData_ = nullptr;
    bool                        Unique_ = false;
};

#endif
