/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
#include <vector>
#include <sstream>
#include <ios>
using namespace std;
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// Errors

namespace tiff_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "file smaller than expected",
    "file header",
    "IFD tag type",
    "IFD tag count",
    "FirstIFDOffset",
    "NrOfDirectories",
    "Expected data size is bigger than real file size",
};

enum code : uint8_t
{
    BufferOverflow,
    Header,
    IfdTagType,
    IfdTagCount,
    FirstIFDOffset,
    NrOfDirectories,
    DataSize,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unparsable

namespace unsupported
{

static const char* MessageText[] =
{
    // Unsupported
    "IFD tag values not same",
    "NewSubfileType",
    "SubfileType",
    "ImageWidth",
    "BitsPerSample",
    "ImageHeight",
    "PhotometricInterpretation",
    "FillOrder",
    "Orientation",
    "PlanarConfiguration",
    "ExtraSamples",
    "ExtraSamples / SamplesPerPixel coherency",
    "SamplesPerPixel",
    "empty StripOffsets",
    "empty StripBytesCounts",
    "SampleFormat",
    "StripOffsets / StripBytesCounts sizes coherency",
    "non continuous strip offsets",
    "IFD tag",
    "IFD unknown tag",
    "incoherent StripOffsets",
    "flavor (PhotometricInterpretation / SamplesPerPixel / BitsPerSample / Endianness combination)",
    "pixels in slice not on a 32-bit boundary",
    "Compression",
    "RowsPerStrip",
    "RowsPerStrip / StripBytesCounts coherency",
};

enum code : uint8_t
{
    IfdTag_ValuesNotSame,
    NewSubfileType,
    SubfileType,
    ImageWidth,
    ImageHeight,
    BitsPerSample,
    PhotometricInterpretation,
    FillOrder,
    Orientation,
    PlanarConfiguration,
    ExtraSamples,
    ExtraSamples_SamplesPerPixel,
    SamplesPerPixel,
    StripOffsets_Empty,
    StripBytesCounts_Empty,
    SampleFormat,
    StripOffsets_StripBytesCounts_Sizes,
    StripOffsets_NonContinuous,
    IfdTag,
    IfdUnknownTag,
    StripOffsets_Incoherent,
    Flavor,
    PixelBoundaries,
    Compression,
    RowsPerStrip,
    RowsPerStrip_StripBytesCounts,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unsupported

const char** ErrorTexts[] =
{
    undecodable::MessageText,
    unsupported::MessageText,
    nullptr,
    nullptr,
};

static_assert(error::type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // tiff_issue

using namespace tiff_issue;

//---------------------------------------------------------------------------
// Tested cases
struct tiff_tested
{
    colorspace                  ColorSpace;
    bitdepth                    BitsPerSample;
    endianness                  Endianness;

    // Additional info not for comparison
    uint8_t                     BytesPerBlock;
    uint8_t                     PixelsPerBlock;

    bool                        Planar; // Planes after each other, block info is of a line of a plane

    bool operator == (const tiff_tested &Value) const
    {
        return BitsPerSample == Value.BitsPerSample
            && Endianness == Value.Endianness
            && ColorSpace == Value.ColorSpace
            && Planar == Value.Planar
            ;
    }
};
struct dpx_also
{
    tiff_tested                 Test;
    tiff::flavor                Flavor;
};


struct tiff_tested TIFF_Tested[] =
{
    { colorspace::RGB     ,  8, endianness::LE, 3, 1 }, // 1x3x 8-bit in 3x 8-bit
    { colorspace::RGB     , 16, endianness::LE, 6, 1 }, // 1x3x16-bit in 3x16-bit
    { colorspace::RGB     , 16, endianness::BE, 6, 1 }, // 1x3x16-bit in 3x16-bit
    { colorspace::RGBA    ,  8, endianness::LE, 4, 1 }, // 1x4x 8-bit in 4x 8-bit
    { colorspace::RGBA    , 16, endianness::LE, 8, 1 }, // 1x4x16-bit in 4x16-bit
    { colorspace::Y       ,  8, endianness::LE, 1, 1 }, // 1x4x 8-bit in 1x 8-bit
    { colorspace::Y       , 16, endianness::LE, 2, 1 }, // 1x1x16-bit in 1x16-bit
    { colorspace::Y       , 16, endianness::BE, 2, 1 }, // 1x1x16-bit in 1x16-bit
    { colorspace::RGB     , 12, endianness::LE, 9, 2 }, // 2x3x12-bit in 9x 8-bit, packed from the MSB
    { colorspace::Y       , 12, endianness::LE, 3, 2 }, // 2x1x12-bit in 3x 8-bit, packed from the MSB
    { colorspace::Y       , 10, endianness::LE, 5, 4 }, // 4x1x10-bit in 5x 8-bit, packed from the MSB
    { colorspace::RGB     , 10, endianness::LE,15, 4 }, // 4x3x10-bit in 15x 8-bit, packed from the MSB
    { colorspace::Y       , 14, endianness::LE, 7, 4 }, // 4x1x14-bit in 7x 8-bit, packed from the MSB
    { colorspace::RGB     , 14, endianness::LE,21, 4 }, // 4x3x14-bit in 21x 8-bit, packed from the MSB
    { colorspace::Y       ,  2, endianness::LE, 1, 4 }, // 4x1x 2-bit in 1x 8-bit, packed from the MSB
    { colorspace::Y       ,  4, endianness::LE, 1, 2 }, // 2x1x 4-bit in 1x 8-bit, packed from the MSB
    { colorspace::Y       ,  6, endianness::LE, 3, 4 }, // 4x1x 6-bit in 3x 8-bit, packed from the MSB
    { colorspace::RGB     ,  2, endianness::LE, 3, 4 }, // 4x3x 2-bit in 3x 8-bit, packed from the MSB
    { colorspace::RGB     ,  4, endianness::LE, 3, 2 }, // 2x3x 4-bit in 3x 8-bit, packed from the MSB
    { colorspace::RGB     ,  6, endianness::LE, 9, 4 }, // 4x3x 6-bit in 9x 8-bit, packed from the MSB
    { colorspace::RGB     ,  8, endianness::LE, 1, 1, true }, // 1x1x 8-bit in 1x 8-bit per plane
    { colorspace::RGB     , 16, endianness::LE, 2, 1, true }, // 1x1x16-bit in 1x16-bit per plane
    { colorspace::RGB     , 16, endianness::BE, 2, 1, true }, // 1x1x16-bit in 1x16-bit per plane
    { colorspace::RGB     , 10, endianness::LE, 5, 4, true }, // 4x1x10-bit in 5x 8-bit per plane, packed from the MSB
    { colorspace::RGB     , 12, endianness::LE, 3, 2, true }, // 2x1x12-bit in 3x 8-bit per plane, packed from the MSB
    { colorspace::RGB     , 14, endianness::LE, 7, 4, true }, // 4x1x14-bit in 7x 8-bit per plane, packed from the MSB
    { colorspace::RGB     ,  2, endianness::LE, 1, 4, true }, // 4x1x 2-bit in 1x 8-bit per plane, packed from the MSB
    { colorspace::RGB     ,  4, endianness::LE, 1, 2, true }, // 2x1x 4-bit in 1x 8-bit per plane, packed from the MSB
    { colorspace::RGB     ,  6, endianness::LE, 3, 4, true }, // 4x1x 6-bit in 3x 8-bit per plane, packed from the MSB
};
static_assert(tiff::flavor_Max == sizeof(TIFF_Tested) / sizeof(tiff_tested), IncoherencyMessage);

struct dpx_also TIFF_Also[] =
{
    { { colorspace::RGB      ,  8, endianness::BE, 3 }, tiff::flavor::Raw_RGB_8_U            },
    { { colorspace::RGBA     ,  8, endianness::BE, 4 }, tiff::flavor::Raw_RGBA_8_U           },
    { { colorspace::Y        ,  8, endianness::BE, 1 }, tiff::flavor::Raw_Y_8_U              },
    { { colorspace::RGB      , 12, endianness::BE, 9 }, tiff::flavor::Raw_RGB_12_U           }, // Packed samples are a bit stream, not byte swapped
    { { colorspace::Y        , 12, endianness::BE, 3 }, tiff::flavor::Raw_Y_12_U             },
    { { colorspace::Y        , 10, endianness::BE, 5 }, tiff::flavor::Raw_Y_10_U             },
    { { colorspace::RGB      , 10, endianness::BE,15 }, tiff::flavor::Raw_RGB_10_U           },
    { { colorspace::Y        , 14, endianness::BE, 7 }, tiff::flavor::Raw_Y_14_U             },
    { { colorspace::RGB      , 14, endianness::BE,21 }, tiff::flavor::Raw_RGB_14_U           },
    { { colorspace::Y        ,  2, endianness::BE, 1 }, tiff::flavor::Raw_Y_2_U              },
    { { colorspace::Y        ,  4, endianness::BE, 1 }, tiff::flavor::Raw_Y_4_U              },
    { { colorspace::Y        ,  6, endianness::BE, 3 }, tiff::flavor::Raw_Y_6_U              },
    { { colorspace::RGB      ,  2, endianness::BE, 3 }, tiff::flavor::Raw_RGB_2_U            },
    { { colorspace::RGB      ,  4, endianness::BE, 3 }, tiff::flavor::Raw_RGB_4_U            },
    { { colorspace::RGB      ,  6, endianness::BE, 9 }, tiff::flavor::Raw_RGB_6_U            },
    { { colorspace::RGB      ,  8, endianness::BE, 1, 1, true }, tiff::flavor::Raw_RGB_8_U_Planar  },
    { { colorspace::RGB      , 10, endianness::BE, 5, 4, true }, tiff::flavor::Raw_RGB_10_U_Planar },
    { { colorspace::RGB      , 12, endianness::BE, 3, 2, true }, tiff::flavor::Raw_RGB_12_U_Planar },
    { { colorspace::RGB      , 14, endianness::BE, 7, 4, true }, tiff::flavor::Raw_RGB_14_U_Planar },
    { { colorspace::RGB      ,  2, endianness::BE, 1, 4, true }, tiff::flavor::Raw_RGB_2_U_Planar  },
    { { colorspace::RGB      ,  4, endianness::BE, 1, 2, true }, tiff::flavor::Raw_RGB_4_U_Planar  },
    { { colorspace::RGB      ,  6, endianness::BE, 3, 4, true }, tiff::flavor::Raw_RGB_6_U_Planar  },
};

//***************************************************************************
// TIFF
//***************************************************************************

//---------------------------------------------------------------------------
tiff::tiff(errors* Errors_Source) :
    input_base_uncompressed_video(Errors_Source, Parser_TIFF, true)
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
        XMP = 700,
        Copyright = 33432,
        IPTC = 33723,
        Photoshop = 34377,
        ExifIFD = 34665,
        ICCProfile = 34675,
        GPSIFD = 34853,
        PrintIM = 50341,
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
                    Unsupported(unsupported::IfdTag_ValuesNotSame);
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
                    Unsupported(unsupported::IfdTag_ValuesNotSame);
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
                    Unsupported(unsupported::IfdTag_ValuesNotSame);
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
        Undecodable(undecodable::IfdTagType);
        Buffer_Offset += 8;
        return 0;
    }
    uint32_t Count = Get_X4();
    if (!Count)
    {
        Undecodable(undecodable::IfdTagCount);
        Buffer_Offset += 4;
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
void tiff::ParseBuffer()
{
    if (Buffer.Size() < 8)
        return;

    tiff_tested Current;

    Buffer_Offset = 0;
    uint32_t Magic = Get_B4();
    switch (Magic)
    {
        case 0x49492A00: // "II" + 42 in 16-bit hex
            IsBigEndian = false;
            Current.Endianness = endianness::LE;
            break;
        case 0x4D4D002A: // "MM" + 42 in 16-bit hex
            IsBigEndian = true;
            Current.Endianness = endianness::BE;
            break;
        default:
            Undecodable(undecodable::Header);
            return;
    }
    SetDetected();
    uint32_t FirstIFDOffset = Get_X4();
    if (FirstIFDOffset > Buffer.Size())
    {
        Undecodable(undecodable::FirstIFDOffset);
        return;
    }
    if (FirstIFDOffset != 8)
        Buffer_Offset = FirstIFDOffset;

    #define CASE_2(_ELEMENT, _VALUE) \
        case _ELEMENT: _VALUE = Get_Element(); break;
    #define CASE_3(_ELEMENT, _VALUE, _PRESENT) \
        case _ELEMENT: _VALUE = Get_Element(); _PRESENT = true; break;
    #define CASE_P(_ELEMENT, _PRESENT) \
        case _ELEMENT: Get_Element(); _PRESENT = true; break;
    #define CASE_V(_ELEMENT, _VALUE) \
        case _ELEMENT: Get_Element(&_VALUE); break;

    uint32_t NrOfDirectories = Get_X2();
    if (Buffer_Offset + 12 * NrOfDirectories + 4 > Buffer.Size()) // 12 per directory + 4 for next IFD offset
    {
        Undecodable(undecodable::NrOfDirectories);
        return;
    }
    bool UnsupportedKnownIFD = false;
    bool UnsupportedUnknownIFD = false;

    uint32_t NewSubfileType = 0;
    bool     SubfileType_IsPresent = false;
    uint32_t Width;
    bool     Width_IsPresent = false;
    uint32_t Height;
    bool     Height_IsPresent = false;
    uint32_t BitsPerSample;
    bool     BitsPerSample_IsPresent = false;
    uint32_t PhotometricInterpretation;
    bool     PhotometricInterpretation_IsPresent = false;
    uint32_t Compression;
    bool     Compression_IsPresent = false;
    uint32_t FillOrder = 1;
    vector<uint32_t> StripOffsets;
    uint32_t Orientation = 1;
    uint32_t SamplesPerPixel = 1;
    uint32_t RowsPerStrip = (uint32_t)-1; // Default is 2^32-1, one strip
    vector<uint32_t> StripBytesCounts;
    uint32_t PlanarConfiguration = 1;
    uint32_t ExtraSamples;
    bool     ExtraSamples_IsPresent = false;
    uint32_t SampleFormat = 1;
    for (uint32_t i = 0; i < NrOfDirectories; i++)
    {
        uint16_t Tag = Get_X2();
        switch (Tag)
        {
            CASE_2 (e::NewSubfileType               , NewSubfileType);
            CASE_P (e::SubfileType                  , SubfileType_IsPresent);
            CASE_3 (e::ImageWidth                   , Width, Width_IsPresent);
            CASE_3 (e::ImageHeight                  , Height, Height_IsPresent);
            CASE_3 (e::BitsPerSample                , BitsPerSample, BitsPerSample_IsPresent);
            CASE_3 (e::PhotometricInterpretation    , PhotometricInterpretation, PhotometricInterpretation_IsPresent);
            CASE_3 (e::Compression                  , Compression, Compression_IsPresent);
            CASE_2 (e::FillOrder                    , FillOrder);
            CASE_V (e::StripOffsets                 , StripOffsets);
            CASE_2 (e::Orientation                  , Orientation);
            CASE_2 (e::SamplesPerPixel              , SamplesPerPixel);
            CASE_V (e::StripByteCounts              , StripBytesCounts);
            CASE_2 (e::PlanarConfiguration          , PlanarConfiguration);
            CASE_3 (e::ExtraSamples                 , ExtraSamples, ExtraSamples_IsPresent);
            CASE_2 (e::SampleFormat                 , SampleFormat);
            case    e::RowsPerStrip                 :
                                                        {
                                                        auto Type = Get_X2();
                                                        auto Count = Get_X4();
                                                        if (Count == 1 && (Type == Byte || Type == Short || Type == Long))
                                                        {
                                                            RowsPerStrip = Get_ElementValue(Count, TagType_Size[Type], nullptr);
                                                            Buffer_Offset += 4 - TagType_Size[Type];
                                                        }
                                                        else
                                                        {
                                                            RowsPerStrip = 0; // Other forms are read differently by FFmpeg
                                                            Buffer_Offset += 4;
                                                        }
                                                        }
                                                        break;
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
            case    e::XMP                          :
            case    e::Copyright                    :
            case    e::IPTC                         :
            case    e::Photoshop                    :
            case    e::ExifIFD                      :
            case    e::GPSIFD                       :
            case    e::PrintIM                      :
                                                        Buffer_Offset += 10;
                                                        break;
            case    e::ICCProfile                   :
                                                        {
                                                        Buffer_Offset += 2; // Type
                                                        uint32_t Count = Get_X4();
                                                        uint32_t Offset = Get_X4();
                                                        if (Count > 4 && (Offset > Buffer.Size() || Count > Buffer.Size() - Offset))
                                                            Undecodable(undecodable::BufferOverflow); // FFmpeg rejects a profile outside the file
                                                        }
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
                                                        Buffer_Offset += 10;
                                                        UnsupportedKnownIFD = true;
                                                        break;
            default:
                                                        Buffer_Offset += 10;
                                                        UnsupportedUnknownIFD = true;
                                                        break;
        }
    }
    Get_X4(); // IFDOffset

    if (NewSubfileType)
        Unsupported(unsupported::NewSubfileType);
    if (SubfileType_IsPresent)
        Unsupported(unsupported::SubfileType);
    if (!Width_IsPresent)
        Unsupported(unsupported::ImageWidth);
    if (!Height_IsPresent)
        Unsupported(unsupported::ImageHeight);
    if (!BitsPerSample_IsPresent)
        Unsupported(unsupported::BitsPerSample);
    if (!PhotometricInterpretation_IsPresent)
        Unsupported(unsupported::PhotometricInterpretation);
    if (!Compression_IsPresent || Compression != 1)
        Unsupported(unsupported::Compression);
    if (FillOrder != 1)
        Unsupported(unsupported::FillOrder);
    if (StripOffsets.empty())
        Unsupported(unsupported::StripOffsets_Empty);
    if (Orientation != 1)
        Unsupported(unsupported::Orientation);
    if (!RowsPerStrip)
        Unsupported(unsupported::RowsPerStrip);
    if (StripBytesCounts.empty())
        Unsupported(unsupported::StripBytesCounts_Empty);
    if (StripOffsets.size() != StripBytesCounts.size())
        Unsupported(unsupported::StripOffsets_StripBytesCounts_Sizes);
    if (PlanarConfiguration != 1 && PlanarConfiguration != 2)
        Unsupported(unsupported::PlanarConfiguration);
    Current.Planar = PlanarConfiguration == 2 && SamplesPerPixel > 1;
    if (SampleFormat != 1)
        Unsupported(unsupported::SampleFormat);
    if (UnsupportedKnownIFD)
        Unsupported(unsupported::IfdTag);
    if (UnsupportedUnknownIFD)
        Unsupported(unsupported::IfdUnknownTag);
    switch (PhotometricInterpretation)
    {
    case  1: // Y
        switch (SamplesPerPixel)
        {
        case  1: Current.ColorSpace = colorspace::Y; break;
        default: Current.ColorSpace = (decltype(Current.ColorSpace))-1;
        }
        break;
    case  2 : // RGB / RGBA
        switch (SamplesPerPixel)
        {
        case  3: Current.ColorSpace = colorspace::RGB; break;
        case  4: Current.ColorSpace = colorspace::RGBA; break;
        default: Current.ColorSpace = (decltype(Current.ColorSpace))-1;
        }
        break;
    default: Current.ColorSpace = (decltype(Current.ColorSpace))-1;
    }
    switch (Current.ColorSpace)
    {
    case colorspace::RGBA:
        if (!ExtraSamples_IsPresent || ExtraSamples != 2)
            Unsupported(unsupported::ExtraSamples);
        break;
    default:;
        if (ExtraSamples_IsPresent)
            Unsupported(unsupported::ExtraSamples);
    }
    if (!BitsPerSample_IsPresent || (BitsPerSample_IsPresent && BitsPerSample > (decltype(tiff_tested::BitsPerSample))-1))
    {
        if (BitsPerSample_IsPresent)
            Unsupported(unsupported::Flavor);
        return;
    }
    Current.BitsPerSample = (decltype(tiff_tested::BitsPerSample))BitsPerSample;
    for (const auto& TIFF_Tested_Item : TIFF_Tested)
    {
        if (TIFF_Tested_Item == Current)
        {
            Flavor = (decltype(Flavor))(&TIFF_Tested_Item - TIFF_Tested);
            break;
        }
    }
    if (Flavor == (decltype(Flavor))-1)
    {
        for (const auto& TIFF_Also_Item : TIFF_Also)
        {
            if (TIFF_Also_Item.Test == Current)
            {
                Flavor = (decltype(Flavor))TIFF_Also_Item.Flavor;
                break;
            }
        }
    }
    if (Flavor == (decltype(Flavor))-1)
        Unsupported(unsupported::Flavor);
    if (HasErrors())
        return;
    if (Flavor == (decltype(Flavor))-1 || StripOffsets.empty() || StripOffsets.size() != StripBytesCounts.size())
        return; // Not reported as unsupported if not encoding (e.g. --info)

    // StripOffsets / StripBytesCounts / RowsPerStrip
    uint32_t StripOffsets_Last = StripOffsets[0] + StripBytesCounts[0];
    size_t StripBytesCount_Size = StripBytesCounts.size();
    for (size_t i = 1; i < StripBytesCount_Size; i++)
    {
        if (StripOffsets_Last != StripOffsets[i])
            Unsupported(unsupported::StripOffsets_NonContinuous);
        StripOffsets_Last += StripBytesCounts[i];
    }

    // FFmpeg reads RowsPerStrip lines from each strip of a plane and the remaining lines from the last one
    uint32_t StripHeight = RowsPerStrip < Height ? RowsPerStrip : Height;
    if (StripHeight)
    {
        size_t Planes = PlaneCount((flavor)Flavor);
        size_t StripsPerPlane = (Height - 1) / StripHeight + 1;
        uint64_t BytesPerLine = ((uint64_t)Width * BytesPerBlock((flavor)Flavor) * 8 / PixelsPerBlock((flavor)Flavor) + 7) / 8; // Line of a plane, on a byte boundary
        uint64_t StripSize = StripHeight * BytesPerLine;
        uint64_t LastStripSize = (Height - (StripsPerPlane - 1) * StripHeight) * BytesPerLine;
        if (StripBytesCount_Size != StripsPerPlane * Planes)
            Unsupported(unsupported::RowsPerStrip_StripBytesCounts);
        else
        {
            for (size_t i = 0; i < StripBytesCount_Size; i++)
                if (StripBytesCounts[i] != ((i + 1) % StripsPerPlane ? StripSize : LastStripSize))
                {
                    Unsupported(unsupported::RowsPerStrip_StripBytesCounts);
                    break;
                }
        }
    }
    else
        Unsupported(unsupported::RowsPerStrip_StripBytesCounts);

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
    if (Current.BitsPerSample > 10)
        slice_x = slice_x * 3 / 2; // 1.5x more slices if 16-bit
    if (slice_x > Width / 2)
        slice_x = Width / 2;
    if (slice_x > Height / 2)
        slice_x = Height / 2;
    if (!slice_x)
        slice_x = 1;

    // Computing which slice count is suitable
    if (Actions[Action_VulkanEncoder])
        slice_x = 0; // Encoder default
    slice_y = slice_x;

    // Computing EndOfImagePadding
    size_t BitsPerLine = Width * BytesPerBlock((flavor)Flavor) * 8 / PixelsPerBlock((flavor)Flavor);
    size_t BytesPerLine = (BitsPerLine + 7) / 8; // Lines begin on a byte boundary
    size_t LineCount = Height * PlaneCount((flavor)Flavor); // Lines of all planes
    size_t OffsetAfterImage = StripOffsets[0] + BytesPerLine * LineCount;
    if (OffsetAfterImage != StripOffsets_Last)
        Unsupported(unsupported::StripOffsets_Incoherent);
    size_t EndOfImagePadding;
    if (OffsetAfterImage > Buffer.Size())
    {
        if (!Actions[Action_AcceptTruncated])
        {
            Undecodable(undecodable::DataSize);
            return;
        }
        EndOfImagePadding = 0;
    }
    else
        EndOfImagePadding = Buffer.Size() - OffsetAfterImage;

    // Can we compress?
    if (!HasErrors())
        SetSupported();

    // Testing padding bits (end of lines not on a byte boundary)
    In.Clear();
    if (IsSupported() && !Actions[Action_AcceptTruncated] && Actions[Action_CheckPadding] && RAWcooked && BitsPerLine % 8)
    {
        auto Data = Buffer.Data() + StripOffsets[0];
        auto Mask = (uint8_t)(0xFF >> (BitsPerLine % 8));
        size_t y = 0;
        for (; y < LineCount; y++)
            if (Data[(y + 1) * BytesPerLine - 1] & Mask)
                break;
        if (y < LineCount)
        {
            // Non-zero padding bits found, storing them
            In.Create(BytesPerLine * LineCount);
            In.SetZero();
            for (; y < LineCount; y++)
            {
                auto i = (y + 1) * BytesPerLine - 1;
                In[i] = Data[i] & Mask;
            }
        }
    }

    // Write RAWcooked file
    if (RAWcooked)
    {
        parse_params Params;
        if (IsSupported())
        {
            Params.BeforeData = Buffer.Data();
            Params.BeforeData_Size = StripOffsets[0];
            Params.AfterData = Buffer.Data() + Buffer.Size() - EndOfImagePadding;
            Params.AfterData_Size = EndOfImagePadding;
            Params.InData = In.Data();
            Params.InData_Size = In.Size();
            Params.InputFile_Size = Buffer.Size();
        }
        ParseRAWcooked(Params);
    }
}

//---------------------------------------------------------------------------
void tiff::BufferOverflow()
{
    Undecodable(undecodable::BufferOverflow);
}

//---------------------------------------------------------------------------
string tiff::Flavor_String()
{
    return TIFF_Flavor_String((uint8_t)Flavor);
}

//---------------------------------------------------------------------------
size_t tiff::BytesPerBlock(tiff::flavor Flavor)
{
    return TIFF_Tested[(size_t)Flavor].BytesPerBlock;
}

//---------------------------------------------------------------------------
size_t tiff::PixelsPerBlock(tiff::flavor Flavor)
{
    return TIFF_Tested[(size_t)Flavor].PixelsPerBlock;
}

//---------------------------------------------------------------------------
size_t tiff::PlaneCount(tiff::flavor Flavor)
{
    const auto& Info = TIFF_Tested[(size_t)Flavor];
    return Info.Planar ? Colorspace2Count(Info.ColorSpace) : 1;
}

//---------------------------------------------------------------------------
string TIFF_Flavor_String(uint8_t Flavor)
{
    const auto& Info = TIFF_Tested[(size_t)Flavor];
    string ToReturn("TIFF/");
    ToReturn += Raw_Flavor_String(Info.BitsPerSample, sign::U, Info.Endianness, Info.ColorSpace);
    if (Info.Planar)
        ToReturn += "/Planar";
    return ToReturn;
}
