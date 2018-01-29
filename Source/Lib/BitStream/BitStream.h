/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
// Read a stream bit per bit
// Can read up to 32 bits at once
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
#ifndef BitStreamH
#define BitStreamH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstdint>
//---------------------------------------------------------------------------

class BitStream
{
public:
    BitStream() {
        Buffer = NULL;
        Buffer_Size = Buffer_Size_Init = 0;
        BufferUnderRun = false;
    }
    BitStream(const uint8_t* Buffer_, size_t Size_) {
        Buffer = Buffer_;
        Buffer_Size = Buffer_Size_Init = Size_ * 8; //Size is in bits
        BufferUnderRun = false;
    }
    ~BitStream() {}

    void Attach(const uint8_t* Buffer_, size_t Size_)
    {
        Buffer = Buffer_;
        Buffer_Size = Buffer_Size_Init = Size_ * 8; //Size is in bits
        BufferUnderRun = false;
    }

    bool  GetB()
    {
        if (Buffer_Size % 8)
        {
            Buffer_Size--;
            return ((LastByte >> (Buffer_Size % 8)) & 0x1) ? true : false;
        }

        if (!Buffer_Size)
        {
            Buffer_Size = 0;
            BufferUnderRun = true;
            return false;
        }

        LastByte = *Buffer;
        Buffer++;
        Buffer_Size--;
        return (LastByte & 0x80) ? true : false;
    }

    uint8_t  Get1(uint8_t HowMany)
    {
        uint8_t ToReturn;
        static const uint8_t Mask[9] =
        {
            0x00,
            0x01, 0x03, 0x07, 0x0f,
            0x1f, 0x3f, 0x7f, 0xff,
        };

        if (HowMany <= (Buffer_Size % 8))
        {
            Buffer_Size -= HowMany;
            return (LastByte >> (Buffer_Size % 8))&Mask[HowMany];
        }

        if (HowMany>Buffer_Size)
        {
            Buffer_Size = 0;
            BufferUnderRun = true;
            return 0;
        }

        uint8_t NewBits = HowMany - (Buffer_Size % 8);
        if (NewBits == 8)
            ToReturn = 0;
        else
            ToReturn = LastByte << NewBits;
        LastByte = *Buffer;
        Buffer++;
        Buffer_Size -= HowMany;
        ToReturn |= (LastByte >> (Buffer_Size % 8))&Mask[NewBits];
        return ToReturn&Mask[HowMany];
    }

    uint16_t Get2(uint8_t HowMany)
    {
        uint16_t ToReturn;
        static const uint16_t Mask[17] =
        {
            0x0000,
            0x0001, 0x0003, 0x0007, 0x000f,
            0x001f, 0x003f, 0x007f, 0x00ff,
            0x01ff, 0x03ff, 0x07ff, 0x0fff,
            0x1fff, 0x3fff, 0x7fff, 0xffff,
        };

        if (HowMany <= (Buffer_Size % 8))
        {
            Buffer_Size -= HowMany;
            return (LastByte >> (Buffer_Size % 8))&Mask[HowMany];
        }

        if (HowMany>Buffer_Size)
        {
            Buffer_Size = 0;
            BufferUnderRun = true;
            return 0;
        }

        uint8_t NewBits = HowMany - (Buffer_Size % 8);
        if (NewBits == 16)
            ToReturn = 0;
        else
            ToReturn = LastByte << NewBits;
        if ((NewBits - 1) >> 3)
        {
            NewBits -= 8;
            ToReturn |= *Buffer << NewBits;
            Buffer++;
        }
        LastByte = *Buffer;
        Buffer++;
        Buffer_Size -= HowMany;
        ToReturn |= (LastByte >> (Buffer_Size % 8))&Mask[NewBits];
        return ToReturn&Mask[HowMany];
    }

    uint32_t Get4(uint8_t HowMany)
    {
        uint32_t ToReturn;
        static const uint32_t Mask[33] =
        {
            0x00000000,
            0x00000001, 0x00000003, 0x00000007, 0x0000000f,
            0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
            0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
            0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
            0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
            0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
            0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff,
            0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff,
        };

        if (HowMany <= (Buffer_Size % 8))
        {
            Buffer_Size -= HowMany;
            return (LastByte >> (Buffer_Size % 8))&Mask[HowMany];
        }

        if (HowMany>Buffer_Size)
        {
            Buffer_Size = 0;
            BufferUnderRun = true;
            return 0;
        }

        uint8_t NewBits = HowMany - (Buffer_Size % 8);
        if (NewBits == 32)
            ToReturn = 0;
        else
            ToReturn = LastByte << NewBits;
        switch ((NewBits - 1) >> 3)
        {
        case 3:    NewBits -= 8;
            ToReturn |= *(Buffer++) << NewBits;
        case 2:    NewBits -= 8;
            ToReturn |= *(Buffer++) << NewBits;
        case 1:    NewBits -= 8;
            ToReturn |= *(Buffer++) << NewBits;
        default:;
        }
        LastByte = *(Buffer++);
        Buffer_Size -= HowMany;
        ToReturn |= (LastByte >> (Buffer_Size % 8))&Mask[NewBits];
        return ToReturn&Mask[HowMany];
    }

    uint64_t Get8(uint8_t HowMany)
    {
        if (HowMany>64)
            return 0; //Not supported
        uint8_t HowMany1, HowMany2;
        uint64_t Value1, Value2;
        if (HowMany>32)
            HowMany1 = HowMany - 32;
        else
            HowMany1 = 0;
        HowMany2 = HowMany - HowMany1;
        Value1 = Get4(HowMany1);
        Value2 = Get4(HowMany2);
        if (BufferUnderRun)
            return 0;
        return Value1 * 0x100000000LL + Value2;
    }

    void Skip(size_t HowMany)
    {
        if (HowMany <= (Buffer_Size % 8))
        {
            Buffer_Size -= HowMany;
            return;
        }

        if (HowMany>Buffer_Size)
        {
            Buffer_Size = 0;
            BufferUnderRun = true;
            return;
        }

        Buffer += (HowMany - (Buffer_Size % 8) - 1) >> 3;
        LastByte = *Buffer;
        Buffer++;
        Buffer_Size -= HowMany;
    }

    bool   PeekB()
    {
        if (Buffer_Size % 8)
            return ((LastByte >> ((Buffer_Size - 1) % 8)) & 0x1) ? true : false;

        if (!Buffer_Size)
        {
            Buffer_Size = 0;
            BufferUnderRun = true;
            return false;
        }

        return ((*Buffer) & 0x80) ? true : false;
    }

    uint8_t  Peek1(uint8_t HowMany)
    {
        uint8_t ToReturn;
        static const uint8_t Mask[9] =
        {
            0x00,
            0x01, 0x03, 0x07, 0x0f,
            0x1f, 0x3f, 0x7f, 0xff,
        };

        if (HowMany <= (Buffer_Size % 8))
            return (LastByte >> ((Buffer_Size - HowMany) % 8))&Mask[HowMany];

        if (HowMany>Buffer_Size)
        {
            Buffer_Size = 0;
            BufferUnderRun = true;
            return 0;
        }

        uint8_t NewBits = HowMany - (Buffer_Size % 8);
        if (NewBits == 8)
            ToReturn = 0;
        else
            ToReturn = LastByte << NewBits;
        ToReturn |= ((*Buffer) >> ((Buffer_Size - HowMany) % 8))&Mask[NewBits];

        return ToReturn&Mask[HowMany];
    }

    uint16_t Peek2(uint8_t HowMany)
    {
        uint16_t ToReturn;
        static const uint16_t Mask[17] =
        {
            0x0000,
            0x0001, 0x0003, 0x0007, 0x000f,
            0x001f, 0x003f, 0x007f, 0x00ff,
            0x01ff, 0x03ff, 0x07ff, 0x0fff,
            0x1fff, 0x3fff, 0x7fff, 0xffff,
        };

        if (HowMany <= (Buffer_Size % 8))
            return (LastByte >> ((Buffer_Size - HowMany) % 8))&Mask[HowMany];

        if (HowMany>Buffer_Size)
        {
            Buffer_Size = 0;
            BufferUnderRun = true;
            return 0;
        }

        const uint8_t* Buffer_Save = Buffer;

        uint8_t NewBits = HowMany - (Buffer_Size % 8);
        if (NewBits == 16)
            ToReturn = 0;
        else
            ToReturn = LastByte << NewBits;
        if ((NewBits - 1) >> 3)
        {
            NewBits -= 8;
            ToReturn |= *Buffer << NewBits;
            Buffer++;
        }
        ToReturn |= ((*Buffer) >> ((Buffer_Size - HowMany) % 8))&Mask[NewBits];

        Buffer = Buffer_Save;

        return ToReturn&Mask[HowMany];
    }

    uint32_t Peek4(uint8_t HowMany)
    {
        uint32_t ToReturn;
        static const uint32_t Mask[33] =
        {
            0x00000000,
            0x00000001, 0x00000003, 0x00000007, 0x0000000f,
            0x0000001f, 0x0000003f, 0x0000007f, 0x000000ff,
            0x000001ff, 0x000003ff, 0x000007ff, 0x00000fff,
            0x00001fff, 0x00003fff, 0x00007fff, 0x0000ffff,
            0x0001ffff, 0x0003ffff, 0x0007ffff, 0x000fffff,
            0x001fffff, 0x003fffff, 0x007fffff, 0x00ffffff,
            0x01ffffff, 0x03ffffff, 0x07ffffff, 0x0fffffff,
            0x1fffffff, 0x3fffffff, 0x7fffffff, 0xffffffff,
        };

        if (HowMany <= (Buffer_Size % 8))
            return (LastByte >> ((Buffer_Size - HowMany) % 8))&Mask[HowMany];

        if (HowMany>Buffer_Size)
        {
            Buffer_Size = 0;
            BufferUnderRun = true;
            return 0;
        }

        const uint8_t* Buffer_Save = Buffer;

        uint8_t NewBits = HowMany - (Buffer_Size % 8);
        if (NewBits == 32)
            ToReturn = 0;
        else
            ToReturn = LastByte << NewBits;
        switch ((NewBits - 1) >> 3)
        {
        case 3:    NewBits -= 8;
            ToReturn |= *Buffer << NewBits;
            Buffer++;
        case 2:    NewBits -= 8;
            ToReturn |= *Buffer << NewBits;
            Buffer++;
        case 1:    NewBits -= 8;
            ToReturn |= *Buffer << NewBits;
            Buffer++;
        default:;
        }
        ToReturn |= ((*Buffer) >> ((Buffer_Size - HowMany) % 8))&Mask[NewBits];

        Buffer = Buffer_Save;

        return ToReturn&Mask[HowMany];
    }

    uint64_t Peek8(uint8_t HowMany)
    {
        return (uint64_t)Peek4(HowMany); //Not yet implemented
    }

    inline size_t Remain() const //How many bits remain?
    {
        return Buffer_Size;
    }

    inline void Byte_Align()
    {
        Skip(Buffer_Size % 8);
    }

    inline size_t Offset_Get() const
    {
        return (Buffer_Size_Init - Buffer_Size) / 8 + (((Buffer_Size_Init - Buffer_Size) % 8) ? 1 : 0);
    }

    inline size_t BitOffset_Get() const
    {
        return Buffer_Size % 8;
    }

    inline size_t OffsetBeforeLastCall_Get()  const //No more valid
    {
        return Buffer_Size % 8;
    }

private:
    const uint8_t*    Buffer;
    size_t          Buffer_Size;
    size_t          Buffer_Size_Init;
    uint8_t           LastByte;
public:
    bool            BufferUnderRun;
};

#endif
