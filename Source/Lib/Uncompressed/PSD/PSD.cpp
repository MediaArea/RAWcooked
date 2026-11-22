/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Uncompressed/PSD/PSD.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
using namespace std;
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// Errors

namespace psd_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "file smaller than expected",
    "Version",
    "Expected data size is bigger than real file size",
};

enum code : uint8_t
{
    BufferOverflow,
    Version,
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
    "PSB (large document format)",
    "Width",
    "Height",
    "Depth",
    "ColorMode",
    "Channels (alpha or extra channels)",
    "Compression of the merged image (only raw is supported)",
};

enum code : uint8_t
{
    Version_PSB,
    Width,
    Height,
    Depth,
    ColorMode,
    Channels,
    Compression,
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

} // psd_issue

using namespace psd_issue;

//---------------------------------------------------------------------------
// Tested cases
struct psd_tested
{
    colorspace                  ColorSpace;
    bitdepth                    BitsPerSample;
    tiff::flavor                TIFF_Flavor; // Same pixel layout
};

static const psd_tested PSD_Tested[] =
{
    { colorspace::RGB     ,  8, tiff::flavor::Raw_RGB_8_U_Planar     },
    { colorspace::RGB     , 16, tiff::flavor::Raw_RGB_16_U_BE_Planar },
    { colorspace::Y       ,  8, tiff::flavor::Raw_Y_8_U              },
    { colorspace::Y       , 16, tiff::flavor::Raw_Y_16_U_BE          },
};
static_assert(psd::flavor_Max == sizeof(PSD_Tested) / sizeof(psd_tested), IncoherencyMessage);

//---------------------------------------------------------------------------
enum color_mode : uint16_t
{
    ColorMode_Bitmap = 0,
    ColorMode_Grayscale = 1,
    ColorMode_Indexed = 2,
    ColorMode_RGB = 3,
    ColorMode_CMYK = 4,
    ColorMode_Multichannel = 7,
    ColorMode_Duotone = 8,
    ColorMode_Lab = 9,
};

//***************************************************************************
// PSD
//***************************************************************************

//---------------------------------------------------------------------------
psd::psd(errors* Errors_Source) :
    input_base_uncompressed_video(Errors_Source, Parser_PSD, true)
{
}

//---------------------------------------------------------------------------
void psd::ParseBuffer()
{
    if (Buffer.Size() < 4)
        return;

    Buffer_Offset = 0;
    IsBigEndian = true;
    if (Get_B4() != 0x38425053) // "8BPS"
        return;
    SetDetected();

    // Header
    uint16_t Version = Get_B2();
    Buffer_Offset += 6; // Reserved, kept in the before data
    uint16_t Channels = Get_B2();
    uint32_t Height = Get_B4();
    uint32_t Width = Get_B4();
    uint16_t Depth = Get_B2();
    uint16_t ColorMode = Get_B2();
    if (HasErrors())
        return;
    switch (Version)
    {
        case 1: break;
        case 2: Unsupported(unsupported::Version_PSB); return; // Section lengths and limits differ
        default: Undecodable(undecodable::Version); return;
    }

    // Color mode data, image resources, layer and mask information
    for (int i = 0; i < 3; i++)
    {
        uint32_t Length = Get_B4();
        if (HasErrors())
            return;
        if (Length > Buffer.Size() - Buffer_Offset)
        {
            Undecodable(undecodable::BufferOverflow);
            return;
        }
        Buffer_Offset += Length;
    }

    // Image data
    uint16_t Compression = Get_B2();
    if (HasErrors())
        return;
    size_t ImageOffset = Buffer_Offset;

    // Flavor
    psd_tested Current;
    Current.ColorSpace = (colorspace)-1;
    Current.BitsPerSample = 0;
    if (!Width || Width > 30000) // PSD limit
        Unsupported(unsupported::Width);
    if (!Height || Height > 30000)
        Unsupported(unsupported::Height);
    switch (Depth)
    {
        case  8:
        case 16: Current.BitsPerSample = (bitdepth)Depth; break;
        default: Unsupported(unsupported::Depth);
    }
    size_t ColorChannels = 0;
    switch (ColorMode)
    {
        case ColorMode_Grayscale:
        case ColorMode_Duotone: // The duotone specification is in the color mode data, the image is grayscale
            Current.ColorSpace = colorspace::Y;
            ColorChannels = 1;
            break;
        case ColorMode_RGB:
            Current.ColorSpace = colorspace::RGB;
            ColorChannels = 3;
            break;
        default: Unsupported(unsupported::ColorMode);
    }
    if (ColorChannels && Channels != ColorChannels)
        Unsupported(unsupported::Channels);
    if (Compression)
        Unsupported(unsupported::Compression);
    for (const auto& PSD_Tested_Item : PSD_Tested)
    {
        if (PSD_Tested_Item.ColorSpace == Current.ColorSpace && PSD_Tested_Item.BitsPerSample == Current.BitsPerSample)
        {
            Flavor = (decltype(Flavor))(&PSD_Tested_Item - PSD_Tested);
            break;
        }
    }
    if (HasErrors())
        return;
    if (Flavor == (decltype(Flavor))-1)
        return; // Not reported as unsupported if not encoding (e.g. --info)
    FrameWidth = Width;
    FrameHeight = Height;
    FrameBitDepth = Current.BitsPerSample;

    // Merged image, planar
    uint64_t ImageSize = (uint64_t)Channels * Height * Width * (Depth / 8);
    uint64_t OffsetAfterImage = ImageOffset + ImageSize;
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
        EndOfImagePadding = Buffer.Size() - (size_t)OffsetAfterImage;

    // Can we compress?
    if (!HasErrors())
        SetSupported();

    // Write RAWcooked file
    if (RAWcooked)
    {
        parse_params Params;
        if (IsSupported())
        {
            Params.BeforeData = Buffer.Data();
            Params.BeforeData_Size = ImageOffset;
            Params.AfterData = Buffer.Data() + Buffer.Size() - EndOfImagePadding;
            Params.AfterData_Size = EndOfImagePadding;
            Params.InputFile_Size = Buffer.Size();
        }
        ParseRAWcooked(Params);
    }
}

//---------------------------------------------------------------------------
void psd::BufferOverflow()
{
    Undecodable(undecodable::BufferOverflow);
}

//---------------------------------------------------------------------------
string psd::Flavor_String()
{
    return PSD_Flavor_String((uint8_t)Flavor);
}

//---------------------------------------------------------------------------
tiff::flavor psd::TIFF_Flavor(psd::flavor Flavor)
{
    return PSD_Tested[(size_t)Flavor].TIFF_Flavor;
}

//---------------------------------------------------------------------------
string PSD_Flavor_String(uint8_t Flavor)
{
    const auto& Info = PSD_Tested[(size_t)Flavor];
    string ToReturn("PSD/");
    ToReturn += Raw_Flavor_String(Info.BitsPerSample, sign::U, endianness::BE, Info.ColorSpace);
    if (Info.ColorSpace != colorspace::Y)
        ToReturn += "/Planar";
    return ToReturn;
}
