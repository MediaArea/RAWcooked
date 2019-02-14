/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/TIFF/TIFF.h"
#include "Lib/RAWcooked/RAWcooked.h"
#include <vector>
#include <sstream>
#include <ios>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Tested cases
struct tiff_info
{
    tiff::compression           Compression;
    tiff::descriptor            Descriptor;
    uint8_t                     SamplesPerPixel;
    uint8_t                     BitsPerSample;
    tiff::sampleformat          SampleFormat;
    tiff::endianess             Endianess;

    bool operator == (tiff_info const &Value) const
    {
        return Compression == Value.Compression
            && Descriptor == Value.Descriptor
            && SamplesPerPixel == Value.SamplesPerPixel
            && BitsPerSample == Value.BitsPerSample
            && SampleFormat == Value.SampleFormat
            && Endianess == Value.Endianess;
    }
};
struct tiff_tested
{
    tiff_info                   Info;
    tiff::flavor                Flavor;
};

struct tiff_tested TIFF_Tested[] =
{
    { { tiff::Raw   , tiff::RGB     ,  3,  8, tiff::U   , tiff::LE}, tiff::Raw_RGB_8_U              },
    { { tiff::Raw   , tiff::RGB     ,  3, 16, tiff::U   , tiff::BE}, tiff::Raw_RGB_16_U_BE          },
    { { tiff::Raw   , tiff::RGB     ,  3, 16, tiff::U   , tiff::LE}, tiff::Raw_RGB_16_U_LE          },
    { { tiff::Raw   , tiff::RGB     ,  4,  8, tiff::U   , tiff::LE}, tiff::Raw_RGBA_8_U             },
    { { tiff::Raw   , tiff::RGB     ,  4, 16, tiff::U   , tiff::LE}, tiff::Raw_RGBA_16_U_LE         },
    { { tiff::Raw   , tiff::RGB     ,  0,  0, tiff::U   , tiff::LE}, tiff::Style_Max                },
};

//---------------------------------------------------------------------------

//***************************************************************************
// TIFF
//***************************************************************************

//---------------------------------------------------------------------------
tiff::tiff() :
    input_base_uncompressed(Parser_TIFF, true),
    FrameRate(NULL)
{
}

//---------------------------------------------------------------------------
namespace e
{
    enum tag_tag : uint16_t
    {
        NewSubfileType = 254,
        SubfileType = 255,
        ImageWidth = 256,
        ImageHeight = 257,
        BitsPerSample = 258,
        Compression = 259,
        PhotometricInterpretation = 262,
        Threshholding = 263,
        CellWidth = 264,
        CellLength = 265,
        FillOrder = 266,
        DocumentName = 269,
        ImageDescription = 270,
        Make = 271,
        Model = 272,
        StripOffsets = 273,
        Orientation = 274,
        SamplesPerPixel = 277,
        RowsPerStrip = 278,
        StripByteCounts = 279,
        MinSampleValue = 280,
        MaxSampleValue = 281,
        XResolution = 282,
        YResolution = 283,
        PlanarConfiguration = 284,
        PageName = 285,
        XPosition = 286,
        YPosition = 287,
        FreeOffsets = 288,
        FreeByteCounts = 289,
        GrayResponseUnit = 290,
        GrayResponseCurve = 291,
        Group3Options = 292,
        Group4Options = 293,
        ResolutionUnit = 296,
        PageNumber = 297,
        ColorResponseUnit = 300,
        ColorResponseCurve = 301,
        Software = 305,
        DateTime = 306,
        Artist = 315,
        HostComputer = 316,
        Predictor = 317,
        WhitePoint = 318,
        PrimaryChromaticities = 319,
        ColorMap = 320,
        HalftoneHints = 321,
        TileWidth = 322,
        TileLength = 323,
        TileOffsets = 324,
        TileByteCounts = 325,
        InkSet = 332,
        InkNames = 333,
        NumberOfInks = 334,
        DotRange = 336,
        TargetPrinter = 337,
        ExtraSamples = 338,
        SampleFormat = 339,
        SMinSampleValue = 340,
        SMaxSampleValue = 341,
        TransferRange = 342,
        JPEGProc = 512,
        JPEGInterchangeFormat = 513,
        JPEGInterchangeFormatLength = 514,
        JPEGRestartInterval = 515,
        JPEGLosslessPredictors = 517,
        JPEGPointTransforms = 518,
        JPEGQTables = 519,
        JPEGDCTTables = 520,
        JPEGACTTables = 521,
        YCbCrCoefficients = 529,
        YCbCrSubSampling = 530,
        YCbCrPositioning = 531,
        ReferenceBlackWhite = 532,
        Copyright = 33432,
    };
};

//---------------------------------------------------------------------------
enum tag_type : uint16_t
{
    Byte       = 1,
    ASCII      = 2,
    Short      = 3,
    Long       = 4,
    Rational   = 5,
    TagType_Max,
};

//---------------------------------------------------------------------------
static const uint8_t TagType_Size[TagType_Max] =
{
    0,
    1,
    1,
    2,
    4,
    8,
};

//---------------------------------------------------------------------------
uint32_t tiff::Get_ElementValue(uint32_t Count, uint32_t Size, std::vector<uint32_t>* List)
{
    uint32_t ToReturn;
    switch (Size)
    {
        case 1 :
            for (uint32_t i = 0; i < Count; i++)
            {
                uint32_t ToReturn2 = Get_X1();
                if (List)
                    List->push_back(ToReturn2);
                if (!i)
                    ToReturn = ToReturn2;
                else if (!List && ToReturn != ToReturn2)
                    Unsupported("TIFF IFD tag value not same");
            }
            break;
        case 2:
            for (uint32_t i = 0; i < Count; i++)
            {
                uint32_t ToReturn2 = Get_X2();
                if (List)
                    List->push_back(ToReturn2);
                if (!i)
                    ToReturn = ToReturn2;
                else if (!List && ToReturn != ToReturn2)
                    Unsupported("TIFF IFD tag value not same");
            }
            break;
        case 4:
            for (uint32_t i = 0; i < Count; i++)
            {
                uint32_t ToReturn2 = Get_X4();
                if (List)
                    List->push_back(ToReturn2);
                if (!i)
                    ToReturn = ToReturn2;
                else if (!List && ToReturn != ToReturn2)
                    Unsupported("TIFF IFD tag value not same");
            }
            break;
    }
    return ToReturn;
}

//---------------------------------------------------------------------------
uint32_t tiff::Get_Element(std::vector<uint32_t>* List)
{
    uint32_t ToReturn;
    
    tag_type TagType = (tag_type)Get_X2();
    if (!TagType || TagType >= TagType_Max)
    {
        Invalid("TIFF IFD tag type");
        Buffer_Offset += 8;
        return 0;
    }
    uint32_t Count = Get_X4();
    if (!Count)
    {
        Invalid("TIFF IFD tag count");
        Buffer_Offset += 8;
        return 0;
    }

    uint32_t Size = TagType_Size[TagType]*Count;
    if (Size <= 4)
    {
        ToReturn = Get_ElementValue(Count, TagType_Size[TagType], List);
        if (Size < 4)
            Buffer_Offset += 4 - Size;
    }
    else
    {
        uint32_t IFDOffset = Get_X4();
        size_t Buffer_Offset_Save = Buffer_Offset;
        Buffer_Offset = IFDOffset;
        data_content DataContent;
        DataContent.Begin = Buffer_Offset;
        ToReturn = Get_ElementValue(Count, TagType_Size[TagType], List);
        DataContent.End = Buffer_Offset;
        Buffer_Offset = Buffer_Offset_Save;
        DataContents.insert(DataContent);
    }

    return ToReturn;
}

//---------------------------------------------------------------------------
bool tiff::ParseBuffer()
{
    if (Buffer_Size < 8)
        return false;

    Buffer_Offset = 0;
    uint32_t Magic = Get_B4();
    switch (Magic)
    {
        case 0x49492A00: // "II" + 42 in 16-bit hex
            IsBigEndian = false;
            break;
        case 0x4D4D002A: // "MM" + 42 in 16-bit hex
            IsBigEndian = true;
            break;
        default:
            return false;
    }
    IsDetected = true;
    uint32_t FirstIFDOffset = Get_X4();
    if (FirstIFDOffset > Buffer_Size)
        return Invalid("FirstIFDOffset");
    if (FirstIFDOffset != 8)
        Buffer_Offset = FirstIFDOffset;

    uint32_t NewSubfileType = 0;
    uint32_t SubfileType = 0;
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t FillOrder = 1;
    uint32_t Orientation = 1;
    uint32_t RowsPerStrip = (uint32_t)-1;
    uint32_t PlanarConfiguration = 1;
    uint32_t ExtraSamples = (uint32_t)-1;
    vector<uint32_t> StripOffsets;
    vector<uint32_t> StripBytesCounts;
    tiff_info Info;
    Info.Compression = Compression_Max;
    Info.Descriptor = Descriptor_Max;
    Info.SamplesPerPixel = 1;
    Info.BitsPerSample = 1;
    Info.SampleFormat = U;
    Info.Endianess = IsBigEndian?BE:LE;

    #define CASE_2(_ELEMENT, _VALUE) \
        case _ELEMENT: _VALUE = Get_Element(); break;
    #define CASE_3(_ELEMENT, _VALUE, _CAST) \
        case _ELEMENT: _VALUE = (_CAST)Get_Element(); break;
    #define CASE_V(_ELEMENT, _VALUE) \
        case _ELEMENT: Get_Element(&_VALUE); break;

    uint32_t NrOfDirectories = Get_X2();
    if (Buffer_Offset + 12 * NrOfDirectories + 4 > Buffer_Size) // 12 per directory + 4 for next IFD offset
        return Invalid("NrOfDirectories");
    bool InsupportedKnownIFD = false;
    bool InsupportedUnknownIFD = false;
    for (uint32_t i = 0; i < NrOfDirectories; i++)
    {
        uint16_t Tag = Get_X2();
        switch (Tag)
        {
            CASE_2 (e::NewSubfileType               , NewSubfileType);
            CASE_2 (e::SubfileType                  , SubfileType);
            CASE_2 (e::ImageWidth                   , Width);
            CASE_2 (e::ImageHeight                  , Height);
            CASE_2 (e::BitsPerSample                , Info.BitsPerSample);
            CASE_3 (e::PhotometricInterpretation    , Info.Descriptor, descriptor);
            CASE_3 (e::Compression                  , Info.Compression, compression);
            CASE_2 (e::FillOrder                    , FillOrder);
            CASE_V (e::StripOffsets                 , StripOffsets);
            CASE_2 (e::Orientation                  , Orientation);
            CASE_2 (e::SamplesPerPixel              , Info.SamplesPerPixel);
            CASE_2 (e::RowsPerStrip                 , RowsPerStrip);
            CASE_V (e::StripByteCounts              , StripBytesCounts);
            CASE_2 (e::PlanarConfiguration          , PlanarConfiguration);
            CASE_2 (e::ExtraSamples                 , ExtraSamples);
            CASE_3 (e::SampleFormat                 , Info.SampleFormat, sampleformat);
            case    e::Threshholding                :
            case    e::CellWidth                    :
            case    e::CellLength                   :
            case    e::DocumentName                 :
            case    e::ImageDescription             :
            case    e::Make                         :
            case    e::Model                        :
            case    e::MinSampleValue               :
            case    e::MaxSampleValue               :
            case    e::XResolution                  :
            case    e::YResolution                  :
            case    e::PageName                     :
            case    e::XPosition                    :
            case    e::YPosition                    :
            case    e::GrayResponseUnit             :
            case    e::GrayResponseCurve            :
            case    e::ResolutionUnit               :
            case    e::PageNumber                   :
            case    e::ColorResponseUnit            :
            case    e::ColorResponseCurve           :
            case    e::Software                     :
            case    e::DateTime                     :
            case    e::Artist                       :
            case    e::HostComputer                 :
            case    e::WhitePoint                   :
            case    e::PrimaryChromaticities        :
            case    e::HalftoneHints                :
            case    e::SMinSampleValue              :
            case    e::SMaxSampleValue              :
            case    e::TransferRange                :
            case    e::YCbCrCoefficients            :
            case    e::YCbCrSubSampling             :
            case    e::YCbCrPositioning             :
            case    e::ReferenceBlackWhite          :
            case    e::Copyright                    :
                                                        Buffer_Offset += 10;
                                                        break;
            case    e::FreeOffsets                  :
            case    e::FreeByteCounts               :
            case    e::Group3Options                :
            case    e::Group4Options                :
            case    e::Predictor                    :
            case    e::ColorMap                     :
            case    e::TileWidth                    :
            case    e::TileLength                   :
            case    e::TileOffsets                  :
            case    e::TileByteCounts               :
            case    e::InkSet                       :
            case    e::InkNames                     :
            case    e::NumberOfInks                 :
            case    e::DotRange                     :
            case    e::TargetPrinter                :
            case    e::JPEGProc                     :
            case    e::JPEGInterchangeFormat        :
            case    e::JPEGInterchangeFormatLength  :
            case    e::JPEGRestartInterval          :
            case    e::JPEGLosslessPredictors       :
            case    e::JPEGPointTransforms          :
            case    e::JPEGQTables                  :
            case    e::JPEGDCTTables                :
            case    e::JPEGACTTables                :
                                                        InsupportedKnownIFD = false;
                                                        break;
            default:
                                                        InsupportedUnknownIFD = false;
                                                        break;
        }
    }
    uint32_t IFDOffset = Get_X4();

    // Supported?
    for (size_t Tested = 0;;)
    {
        tiff_tested& TIFF_Tested_Item = TIFF_Tested[Tested++];
        if (TIFF_Tested_Item.Flavor == tiff::Style_Max)
            return Unsupported("Flavor (Descriptor / sampleSize / Packing / Endianess combination)");
        tiff_info& I = TIFF_Tested_Item.Info;
        if (I == Info)
        {
            Flavor = TIFF_Tested_Item.Flavor;
            break;
        }
    }

    // Unsupported tag content
    if (NewSubfileType)
        return Unsupported("NewSubfileType value");
    if (SubfileType)
        return Unsupported("NewSubfileType value");
    if (FillOrder != 1)
        return Unsupported("FillOrder value");
    if (Orientation != 1)
        return Unsupported("Orientation value");
    if (PlanarConfiguration != 1)
        return Unsupported("PlanarConfiguration value");
    if (ExtraSamples != (uint32_t)-1 && ExtraSamples != 2)
        return Unsupported("ExtraSamples value");
    if (ExtraSamples != (uint32_t)-1 && (Info.Descriptor != RGB || Info.SamplesPerPixel != 4))
        return Unsupported("ExtraSamples/SamplesPerPixel");

    // StripOffsets / StripBytesCounts / RowsPerStrip
    if (StripOffsets.empty())
        return Unsupported("empty StripOffsets");
    if (StripBytesCounts.empty())
        return Unsupported("empty StripBytesCounts");
    if (StripOffsets.size() != StripBytesCounts.size())
        return Unsupported("StripOffsets/StripBytesCounts sizes");
    uint32_t StripOffsets_Last = StripOffsets[0] + StripBytesCounts[0];
    size_t StripBytesCount_Size = StripBytesCounts.size();
    for (size_t i = 1; i < StripBytesCount_Size; i++)
    {
        if (StripOffsets_Last != StripOffsets[i])
            return Unsupported("non continuous strip offsets");
        StripOffsets_Last += StripBytesCounts[i];
    }

    // Other IFDs
    if (InsupportedKnownIFD)
        return Unsupported("IFD tag");
    if (InsupportedUnknownIFD)
        return Unsupported("IFD unknown tag");
                
    // Slices count
    // Computing optimal count of slices. TODO: agree with everyone about the goal and/or permit multiple formulas
    // Current idea:
    // - have some SIMD compatible slice count (e.g. AVX-512 has 16 32-bit blocks, let's take multiples of 16)
    // - each slice has around 256 KiB of data, there is a similar risk of losing 1 LTO block (correction code per block, to be confirmed but looks like a 256 KiB block size is classic and LTFS 2.4 spec indicates 512 KiB in the example)
    // This leads to:
    // SD: 16 slices (10-bit) or 24 slices (16-bit)
    // HD/2K: 64 slices (10-bit) or 96 slices (16-bit)
    // UHD/4K: 256 slices (10-bit) or 384 slices (16-bit)
    // 
    slice_x = 4;
    if (Width >= 1440) // more than 2/3 of 1920, so e.g. DV100 is included
        slice_x <<= 1;
    if (Width >= 2880) // more than 3/2 of 1920, oversampled HD is not included
        slice_x <<= 1;
    slice_y = slice_x;
    if (Info.BitsPerSample > 10)
        slice_x = slice_x * 3 / 2; // 1.5x more slices if 16-bit

    // Computing which slice count is suitable // TODO: smarter algo, currently only computing in order to have pixels not accross 2 32-bit words
    size_t Slice_Multiplier = PixelsPerBlock((flavor)Flavor);
    if (Slice_Multiplier == 0)
        return Invalid("(Internal error)");
    for (; slice_x; slice_x--)
    {
        if (Width % (slice_x * Slice_Multiplier) == 0)
            break;
    }
    if (slice_x == 0)
        return Unsupported("Pixels in slice not on a 32-bit boundary");

    // Computing EndOfImagePadding
    size_t ContentSize_Multiplier = BitsPerBlock((flavor)Flavor);
    if (ContentSize_Multiplier == 0)
        return Invalid("(Internal error)");
    size_t OffsetAfterImage = StripOffsets[0] + Width * Height * ContentSize_Multiplier / 8;
    if (OffsetAfterImage != StripOffsets_Last)
        return Unsupported("incoherent StripOffsets[]");
    size_t EndOfImagePadding;
    if (OffsetAfterImage > Buffer_Size)
    {
        if (!AcceptTruncated)
            return Invalid("File size is too small, file integrity issue");
        EndOfImagePadding = 0;
    }
    else
        EndOfImagePadding = Buffer_Size - OffsetAfterImage;

    // Write RAWcooked file
    if (RAWcooked)
    {
        RAWcooked->Unique = false;
        RAWcooked->Before = Buffer;
        RAWcooked->Before_Size = StripOffsets[0];
        RAWcooked->After = Buffer + Buffer_Size - EndOfImagePadding;
        RAWcooked->After_Size = EndOfImagePadding;
        RAWcooked->FileSize = Buffer_Size;
        RAWcooked->Parse();
    }

    return ErrorMessage() ? true : false;
}

//---------------------------------------------------------------------------
string tiff::Flavor_String()
{
    return TIFF_Flavor_String(Flavor);
}

//---------------------------------------------------------------------------
size_t tiff::BitsPerBlock(tiff::flavor Flavor)
{
    switch (Flavor)
    {
        case tiff::Raw_RGB_8_U:
                                        return 24;          // 3x8-bit content
        case tiff::Raw_RGB_16_U_BE:
        case tiff::Raw_RGB_16_U_LE:
                                        return 48;          // 3x16-bit content
        case tiff::Raw_RGBA_8_U:
                                        return 32;          // 4x8-bit content
        case tiff::Raw_RGBA_16_U_LE:
                                        return 64;          // 4x16-bit content
        default:
                                        return 0;
    }
}

//---------------------------------------------------------------------------
size_t tiff::PixelsPerBlock(tiff::flavor Flavor)
{
    switch (Flavor)
    {
        case tiff::Raw_RGB_8_U:
        case tiff::Raw_RGB_16_U_BE:
        case tiff::Raw_RGB_16_U_LE:
        case tiff::Raw_RGBA_8_U:
        case tiff::Raw_RGBA_16_U_LE:
                                        return 1;
        default:
                                        return 0;
    }
}

//---------------------------------------------------------------------------
tiff::descriptor tiff::Descriptor(tiff::flavor Flavor)
{
    switch (Flavor)
    {
        case Raw_RGB_8_U:
        case Raw_RGB_16_U_BE:
        case Raw_RGB_16_U_LE:
                                        return RGB;
        case Raw_RGBA_8_U:
        case Raw_RGBA_16_U_LE:
                                        return RGBA;
        default:
                                        return (tiff::descriptor)-1;
    }
}
const char* tiff::Descriptor_String(tiff::flavor Flavor)
{
    tiff::descriptor Value = tiff::Descriptor(Flavor);

    switch (Value)
    {
        case RGB:
                                        return "RGB";
        case RGBA:
                                        return "RGBA";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
uint8_t tiff::BitsPerSample(tiff::flavor Flavor)
{
    switch (Flavor)
    {
        case Raw_RGB_8_U:
        case Raw_RGBA_8_U:
                                        return 8;
        case Raw_RGB_16_U_BE:
        case Raw_RGB_16_U_LE:
        case Raw_RGBA_16_U_LE:
                                        return 16;
        default:
                                        return 0;
    }
}
const char* tiff::BitsPerSample_String(tiff::flavor Flavor)
{
    uint8_t Value = BitsPerSample(Flavor);

    switch (Value)
    {
        case 8:
                                        return "8";
        case 10:
                                        return "10";
        case 12:
                                        return "12";
        case 16:
                                        return "16";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
tiff::sampleformat tiff::SampleFormat(tiff::flavor Flavor)
{
    switch (Flavor)
    {
        case Raw_RGB_8_U:
        case Raw_RGBA_8_U:
        case Raw_RGB_16_U_BE:
        case Raw_RGB_16_U_LE:
        case Raw_RGBA_16_U_LE:
                                        return U;
        default:
                                        return (sampleformat)-1;
    }
}
const char* tiff::SampleFormat_String(tiff::flavor Flavor)
{
    tiff::sampleformat Value = SampleFormat(Flavor);

    switch (Value)
    {
        case U:
        case Und:
                                        return "U";
        case S:
                                        return "S";
        case F:
                                        return "F";
        default:
                                        return "";
    }
}

//---------------------------------------------------------------------------
tiff::endianess tiff::Endianess(tiff::flavor Flavor)
{
    switch (Flavor)
    {
        case Raw_RGB_16_U_BE:
                                        return BE;
        case Raw_RGB_16_U_LE:
        case Raw_RGBA_16_U_LE:
                                        return LE;
        default:
                                        return (tiff::endianess)-1;
    }
}
const char* tiff::Endianess_String(tiff::flavor Flavor)
{
    tiff::endianess Value = Endianess(Flavor);

    switch (Value)
    {
        case LE:
                                        return "LE";
        case BE:
                                        return "BE";
        default:
                                        return "";
    }
}
//---------------------------------------------------------------------------
string TIFF_Flavor_String(uint8_t Flavor)
{
    string ToReturn("TIFF/Raw/");
    ToReturn += tiff::Descriptor_String((tiff::flavor)Flavor);
    ToReturn += '/';
    ToReturn += tiff::BitsPerSample_String((tiff::flavor)Flavor);
    ToReturn += "bit";
    const char* Value = tiff::SampleFormat_String((tiff::flavor)Flavor);
    if (Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    Value = tiff::Endianess_String((tiff::flavor)Flavor);
    if (Value[0])
    {
        ToReturn += '/';
        ToReturn += Value;
    }
    return ToReturn;
}
