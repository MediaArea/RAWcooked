/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
#include "Lib/ThirdParty/endianness.h"
#include <algorithm>
#include <sstream>
#include <ios>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace dpx_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "file smaller than expected",
    "file header",
    "version number of header format",
    "offset to data",
    "expected data size is bigger than real file size",
};

enum code : uint8_t
{
    BufferOverflow,
    Header,
    VersionNumber,
    OffsetToData,
    DataSize,
    Max
};

} // unparsable

namespace unsupported
{

static const char* MessageText[] =
{
    // Unsupported
    "offset to image data in bytes",
    "encryption key",
    "image orientation",
    "number of image elements",
    "data sign",
    "encoding",
    "end-of-line padding",
    "\"Frame rate of original (frames/s)\" not same as \"Temporal sampling rate or frame rate (Hz)\"",
    "\"Frame rate of original (frames/s)\" or \"Temporal sampling rate or frame rate (Hz)\" not present",
    "flavor (Descriptor / BitDepth / Packing / Endianness combination)",
    "pixels in slice not on a 32-bit boundary",
    "internal error",
    "(non conforming) alternate end of line non padding",
};

enum code : uint8_t
{
    OffsetToImageData,
    Encryption,
    Orientation,
    NumberOfElements,
    DataSign,
    Encoding,
    EolPadding,
    FrameRate_Incoherent,
    FrameRate_Unavailable,
    Flavor,
    PixelBoundaries,
    InternalError,
    Altern,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unsupported

namespace invalid
{

static const char* MessageText[] =
{
    "offset to image data in bytes",
    "total image file size",
    "version number of header format",
    "ditto key",
    "ditto key is set to \"same as the previous frame\" but header data differs",
    "number of image elements",
    "packing field value",
};

enum code : uint8_t
{
    OffsetToImageData,
    TotalImageFileSize,
    VersionNumber,
    DittoKey,
    DittoKey_NotSame,
    NumberOfElements,
    Packing,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // invalid

const char** ErrorTexts[] =
{
    undecodable::MessageText,
    unsupported::MessageText,
    nullptr,
    invalid::MessageText,
};

static_assert(error::type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // dpx_issue

using namespace dpx_issue;

//---------------------------------------------------------------------------
// Enums
enum class packing : uint8_t
{
    Packed,
    FilledA,
    FilledB,
    Pack3,
};
enum flags : uint8_t
{
    None = 0,
    BlockSpan = 1 << 0,
    VFlip = 1 << 1,
    Altern = 1 << 2,
};

//---------------------------------------------------------------------------
// Tested cases
struct dpx_tested
{
    colorspace                  ColorSpace;
    bitdepth                    BitDepth;
    endianness                  Endianness;
    packing                     Packing;

    bool operator == (const dpx_tested &Value) const
    {
        return ColorSpace == Value.ColorSpace
            && BitDepth == Value.BitDepth
            && Endianness == Value.Endianness
            && Packing == Value.Packing
            ;
    }
};
struct dpx_info
{
    uint8_t                     PixelsPerBlock;
    uint8_t                     BytesPerBlock;
    uint8_t                     Flags;
};
struct dpx_tested_info
{
    dpx_tested                  Test;
    dpx_info                    Info;
};
struct dpx_also
{
    dpx_tested                  Test;
    dpx::flavor                 Flavor;
};

struct dpx_tested_info DPX_Tested[] =
{
    { { colorspace::RGB     ,  8, endianness::LE, packing::Packed }, { 1,  3, None } },                                         // 1x3x 8-bit in 1x24-bit
    { { colorspace::RGB     , 10, endianness::LE, packing::FilledA}, { 1,  4, None } },                                         // 1x3x10-bit in 1x32-bit including 1x2-bit padding
    { { colorspace::RGB     , 10, endianness::BE, packing::FilledA}, { 1,  4, None } },                                         // 1x3x10-bit in 1x32-bit including 1x2-bit padding
    { { colorspace::RGB     , 12, endianness::LE, packing::FilledA}, { 1,  6, None } },                                         // 1x3x12-bit in 3x16-bit including 3x4-bit padding
    { { colorspace::RGB     , 12, endianness::BE, packing::Packed }, { 8, 36, BlockSpan | VFlip } },                            // 8x3x12-bit in 9x32-bit
    { { colorspace::RGB     , 12, endianness::BE, packing::FilledA}, { 1,  6, None } },                                         // 1x3x12-bit in 3x16-bit including 3x4-bit padding
    { { colorspace::RGB     , 16, endianness::LE, packing::Packed }, { 1,  6, None } },                                         // 1x3x16-bit in 3x16-bit
    { { colorspace::RGB     , 16, endianness::BE, packing::Packed }, { 1,  6, None } },                                         // 1x3x16-bit in 3x16-bit
    { { colorspace::RGBA    ,  8, endianness::LE, packing::Packed }, { 1,  4, None } },                                         // 1x4x 8-bit in 1x32-bit
    { { colorspace::RGBA    , 10, endianness::LE, packing::FilledA}, { 3, 16, None } },                                         // 3x4x10-bit in 3x40-bit including 4x2-bit padding
    { { colorspace::RGBA    , 10, endianness::BE, packing::FilledA}, { 3, 16, None } },                                         // 3x4x10-bit in 3x40-bit including 4x2-bit padding
    { { colorspace::RGBA    , 12, endianness::LE, packing::FilledA}, { 1,  8, None } },                                         // 1x4x12-bit in 4x16-bit including 4x4-bit padding
    { { colorspace::RGBA    , 12, endianness::BE, packing::Packed }, { 2, 12, None } },                                         // 2x4x12-bit in 2x48-bit including 2x4-bit padding
    { { colorspace::RGBA    , 12, endianness::BE, packing::FilledA}, { 1,  8, None } },                                         // 1x4x12-bit in 4x16-bit including 4x4-bit padding
    { { colorspace::RGBA    , 16, endianness::LE, packing::Packed }, { 1,  8, None } },                                         // 1x4x16-bit in 4x16-bit
    { { colorspace::RGBA    , 16, endianness::BE, packing::Packed }, { 1,  8, None } },                                         // 1x4x16-bit in 4x16-bit
    { { colorspace::Y       ,  8, endianness::LE, packing::Packed }, { 1,  1, None } },                                         // 1x1x 8-bit in 1x 8-bit
    { { colorspace::Y       , 10, endianness::BE, packing::FilledA}, { 3,  4, BlockSpan | Altern } },                           // 1x3x10-bit in 1x32-bit including 1x2-bit padding
    { { colorspace::Y       , 10, endianness::BE, packing::FilledB}, { 3,  4, BlockSpan | Altern } },                           // 1x3x10-bit in 1x32-bit including 1x2-bit padding
    { { colorspace::Y       , 12, endianness::BE, packing::Packed }, { 8, 12, BlockSpan | VFlip } },                            // 8x1x12-bit in 3x32-bit
    { { colorspace::Y       , 16, endianness::LE, packing::Packed }, { 1,  2, None } },                                         // 1x1x16-bit in 1x16-bit
    { { colorspace::Y       , 16, endianness::BE, packing::Packed }, { 1,  2, None } },                                         // 1x1x16-bit in 1x16-bit
};
static_assert(dpx::flavor_Max == sizeof(DPX_Tested) / sizeof(dpx_tested_info), IncoherencyMessage);

struct dpx_also DPX_Also[] =
{
    { { colorspace::RGB      ,  8, endianness::LE, packing::FilledA }, dpx::flavor::Raw_RGB_8                 },
    { { colorspace::RGB      ,  8, endianness::BE, packing::Packed  }, dpx::flavor::Raw_RGB_8                 },
    { { colorspace::RGB      ,  8, endianness::BE, packing::FilledA }, dpx::flavor::Raw_RGB_8                 },
    { { colorspace::RGB      , 16, endianness::LE, packing::FilledA }, dpx::flavor::Raw_RGB_16_LE             },
    { { colorspace::RGB      , 16, endianness::BE, packing::FilledA }, dpx::flavor::Raw_RGB_16_BE             },
    { { colorspace::RGBA     ,  8, endianness::LE, packing::FilledA }, dpx::flavor::Raw_RGBA_8                },
    { { colorspace::RGBA     ,  8, endianness::BE, packing::Packed  }, dpx::flavor::Raw_RGBA_8                },
    { { colorspace::RGBA     ,  8, endianness::BE, packing::FilledA }, dpx::flavor::Raw_RGBA_8                },
    { { colorspace::RGBA     , 16, endianness::LE, packing::FilledA }, dpx::flavor::Raw_RGBA_16_LE            },
    { { colorspace::RGBA     , 16, endianness::BE, packing::FilledA }, dpx::flavor::Raw_RGBA_16_BE            },
    { { colorspace::Y        ,  8, endianness::LE, packing::Packed  }, dpx::flavor::Raw_Y_8                   },
    { { colorspace::Y        ,  8, endianness::LE, packing::FilledA }, dpx::flavor::Raw_Y_8                   },
    { { colorspace::Y        ,  8, endianness::BE, packing::Packed  }, dpx::flavor::Raw_Y_8                   },
    { { colorspace::Y        ,  8, endianness::BE, packing::FilledA }, dpx::flavor::Raw_Y_8                   },
    { { colorspace::Y        , 16, endianness::LE, packing::FilledA }, dpx::flavor::Raw_Y_16_LE               },
    { { colorspace::Y        , 16, endianness::BE, packing::FilledA }, dpx::flavor::Raw_Y_16_BE               },
    { { colorspace::Y        , 16, endianness::LE, packing::Pack3   }, dpx::flavor::Raw_Y_16_LE               },
    { { colorspace::Y        , 16, endianness::BE, packing::Pack3   }, dpx::flavor::Raw_Y_16_BE               },
};

//***************************************************************************
// DPX
//***************************************************************************

//---------------------------------------------------------------------------
dpx::dpx(errors* Errors_Source) :
    input_base_uncompressed_video(Errors_Source, Parser_DPX, true)
{
}

//---------------------------------------------------------------------------
dpx::~dpx()
{
    delete[] HeaderCopy;
}

//---------------------------------------------------------------------------
void dpx::CopyCommonParser(const input_base_uncompressed& Parser)
{
    const dpx& DPX = (const dpx&)Parser;

    // Comparison
    if (DPX.HeaderCopy) {
        HeaderCopy_Info = DPX.HeaderCopy_Info;
        HeaderCopy = new uint8_t[2048];
        size_t HeaderCopy_Size = (HeaderCopy_Info & 0xFFF) + 1;
        memcpy(HeaderCopy, DPX.HeaderCopy, HeaderCopy_Size);
    }

    // Edits
    Edits = DPX.Edits;

    // Temp
    In = DPX.In;
    In_FirstNonZero = DPX.In_FirstNonZero;
}

enum edit_type : uint8_t
{
    Edit_S,
    Edit_U,
    Edit_F,
};

struct edit_list {
    unsigned    Offset;
    unsigned    Size;
    edit_type   Type;
    const char* Name;
    const char* Description;
};
static edit_list EditList[] = {
    // 5.1 File Information Header
    {    8,    8, Edit_S,   "Version number", nullptr },
    {   16,    4, Edit_U,   "Total image file size", nullptr },
    {   20,    4, Edit_U,   "Ditto key", "0 = same as previous frame; 1 = new"},
    {   24,    4, Edit_U,   "Generic section header length", nullptr },
    {   28,    4, Edit_U,   "Industry specific header length", nullptr },
    {   32,    4, Edit_U,   "User-defined header length", nullptr },
    {   36,  100, Edit_S,   "Image filename", nullptr },
    {  136,   24, Edit_S,   "Creation date/time", "yyyy:mm:dd:hh:mm:ssLTZ" },
    {  160,  100, Edit_S,   "Creator", nullptr },
    {  260,  200, Edit_S,   "Project name", nullptr },
    {  460,  200, Edit_S,   "Copyright", nullptr },
    {  660,    4, Edit_U,   "Encryption key", "0xFFFFFFFF means unencrypted"},
    //{  664,  104,    TBD,   "File Information Header Reserved", nullptr },

    // Image Information Header
    {  768,    2, Edit_U,   "Image orientation", nullptr },
    {  770,    2, Edit_U,   "Number of image elements", nullptr },
    {  772,    4, Edit_U,   "Pixels per line", nullptr },
    {  776,    4, Edit_U,   "Lines per image element", nullptr },

    // Image element 1
    {  780,    4, Edit_U,   "Data sign", "0 = unsigned; 1 = signed" },
    {  784,    4, Edit_U,   "Reference low data code value", nullptr },
    {  788,    4, Edit_F,   "Reference low quantity represented", nullptr },
    {  792,    4, Edit_U,   "Reference high data code value", nullptr },
    {  796,    4, Edit_F,   "Reference high quantity represented", nullptr },
    {  800,    1, Edit_U,   "Descriptor", nullptr },
    {  801,    1, Edit_U,   "Transfer characteristic", nullptr },
    {  802,    1, Edit_U,   "Colorimetric specification", nullptr },
    {  803,    1, Edit_U,   "Bit depth", nullptr },
    {  804,    2, Edit_U,   "Packing", nullptr },
    {  806,    2, Edit_U,   "Encoding", nullptr },
    {  808,    4, Edit_U,   "Offset to data", nullptr },
    {  812,    4, Edit_U,   "End-of-line padding", nullptr },
    {  816,    4, Edit_U,   "End-of-image padding", nullptr },
    {  820,   32, Edit_S,   "Description of image element", nullptr },

    // Image elements 2-8
    // {  852,   72,    TBD,   "Data structure for image element 2", nullptr },
    // {  924,   72,    TBD,   "Data structure for image element 3", nullptr },
    // {  996,   72,    TBD,   "Data structure for image element 4", nullptr },
    // { 1068,   72,    TBD,   "Data structure for image element 5", nullptr },
    // { 1140,   72,    TBD,   "Data structure for image element 6", nullptr },
    // { 1212,   72,    TBD,   "Data structure for image element 7", nullptr },
    // { 1284,   72,    TBD,   "Data structure for image element 8", nullptr },
    // { 1356,   52,    TBD,   "Reserved for future use", nullptr },

    // Image Source Information Header
    { 1408,    4, Edit_U,   "X offset", nullptr },
    { 1412,    4, Edit_U,   "Y offset", nullptr },
    { 1416,    4, Edit_F,   "X center", nullptr },
    { 1420,    4, Edit_F,   "Y center", nullptr },
    { 1424,    4, Edit_U,   "X original size", nullptr },
    { 1428,    4, Edit_U,   "Y original size", nullptr },
    { 1432,  100, Edit_S,   "Source image filename", nullptr },
    { 1532,   24, Edit_S,   "Source image date/time", "yyyy:mm:dd:hh:mm:ssLTZ" },
    { 1556,   32, Edit_S,   "Input device name", nullptr },
    { 1588,   32, Edit_S,   "Input device serial number", nullptr },
    { 1620,    8, Edit_U,   "Border validity XL", nullptr },
    { 1620,    8, Edit_U,   "Border validity XR", nullptr },
    { 1620,    8, Edit_U,   "Border validity YT", nullptr },
    { 1620,    8, Edit_U,   "Border validity YB", nullptr },
    { 1628,    8, Edit_U,   "Pixel aspect ratio horizontal", nullptr },
    { 1628,    8, Edit_U,   "Pixel aspect ratio vertical", nullptr },
    { 1636,    4, Edit_F,   "X scanned size", nullptr },
    { 1640,    4, Edit_F,   "Y scanned size", nullptr },
    // { 1644,   20,    TBD,   "Image Source Information Header Reserved", nullptr },

    // Motion-Picture Film Information Header
    { 1664,    2, Edit_S,   "Film manufacturing ID code", "2 digits from film edge code"},
    { 1666,    2, Edit_S,   "Film type", "2 digits from film edge code" },
    { 1668,    2, Edit_S,   "Offset in perfs", "2 digits from film edge code" },
    { 1670,    6, Edit_S,   "Prefix", "6 digits from film edge code" },
    { 1676,    4, Edit_S,   "Count", "4 digits from film edge code" },
    { 1680,   32, Edit_S,   "Format", "e.g. Academy"},
    { 1712,    4, Edit_U,   "Frame position in sequence", nullptr },
    { 1716,    4, Edit_U,   "Sequence length", "frames"},
    { 1720,    4, Edit_U,   "Held count", "1 = default"},
    { 1724,    4, Edit_F,   "Frame rate of original", nullptr },
    { 1728,    4, Edit_F,   "Shutter angle of camera in degrees", nullptr },
    { 1732,   32, Edit_S,   "Frame identification", nullptr },
    { 1764,  100, Edit_S,   "Slate information", nullptr },
    // { 1864,   56,    TBD,   "Motion-Picture Film Information Header Reserved", nullptr },

    // 6.2 Television Information Header
    { 1920,    4, Edit_U,   "SMPTE time code", nullptr },
    { 1924,    4, Edit_U,   "SMPTE user bits", nullptr },
    { 1928,    1, Edit_U,   "Interlace", "0 = noninterlaced; 1 = 2:1 interlace"},
    { 1929,    1, Edit_U,   "Field number", nullptr },
    { 1930,    1, Edit_U,   "Video signal standard", nullptr },
    // { 1931,    1, Edit_U,   "Zero", nullptr },
    { 1932,    4, Edit_F,   "Horizontal sampling rate", "Hz"},
    { 1936,    4, Edit_F,   "Vertical sampling rate", "Hz" },
    { 1940,    4, Edit_F,   "Temporal sampling rate or frame rate", "Hz" },
    { 1944,    4, Edit_F,   "Time offset from sync to first pixel", "microseconds" },
    { 1948,    4, Edit_F,   "Gamma", nullptr },
    { 1952,    4, Edit_F,   "Black level code value", nullptr },
    { 1956,    4, Edit_F,   "Black gain", nullptr },
    { 1960,    4, Edit_F,   "Breakpoint", nullptr },
    { 1964,    4, Edit_F,   "Reference white level code value", nullptr },
    { 1968,    4, Edit_F,   "Integration time", "s" },
    // { 1972,   76,    TBD,   "Reserved for future use", nullptr },
};

void dpx::AddEditsParser(map<string, string>& Edits)
{
    for (auto it = Edits.begin(); it != Edits.end(); ) {
        bool Found = false;
        string key_lower = it->first;
        transform(key_lower.begin(), key_lower.end(), key_lower.begin(), ::tolower);
        for (auto& EditInfo : EditList) {
            string name_lower = EditInfo.Name;
            transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            if (key_lower == name_lower) {
                switch (EditInfo.Type) {
                case Edit_S:
                {
                    if (it->second.size() > EditInfo.Size)
                        it->second = "value is too long, max is " + to_string(EditInfo.Size);
                    else {
                        auto& EditBuffer = this->Edits[EditInfo.Offset];
                        EditBuffer.Resize(EditInfo.Size);
                        memcpy(EditBuffer.Data(), it->second.c_str(), it->second.size());
                        memset(EditBuffer.Data() + it->second.size(), 0, EditInfo.Size - it->second.size());
                        it = Edits.erase(it);
                        Found = true;
                    }
                    break;
                }
                case Edit_U:
                {
                    try {
                        auto& EditBuffer = this->Edits[EditInfo.Offset];
                        EditBuffer.Resize(EditInfo.Size);

                        uint64_t Value = stoull(it->second);
                        uint8_t* BufferPtr = EditBuffer.Data();

                        if (EditInfo.Size == 1) {
                            *BufferPtr = (uint8_t)Value;
                        } else if (EditInfo.Size == 2) {
                            uint16_t Value16 = (uint16_t)Value;
                            Value16 = IsBigEndian ? htob16(Value16) : htol16(Value16);
                            memcpy(BufferPtr, &Value16, 2);
                        } else if (EditInfo.Size == 4) {
                            uint32_t Value32 = (uint32_t)Value;
                            Value32 = IsBigEndian ? htob32(Value32) : htol32(Value32);
                            memcpy(BufferPtr, &Value32, 4);
                        } else if (EditInfo.Size == 8) {
                            uint64_t Value64 = Value;
                            Value64 = IsBigEndian ? htob64(Value64) : htol64(Value64);
                            memcpy(BufferPtr, &Value64, 8);
                        } else {
                            it->second = "unsupported size: " + to_string(EditInfo.Size);
                            break;
                        }
                        it = Edits.erase(it);
                        Found = true;
                    } catch (const exception&) {
                        it->second = "invalid unsigned integer value";
                    }
                    break;
                }
                case Edit_F:
                {
                    try {
                        auto& EditBuffer = this->Edits[EditInfo.Offset];
                        EditBuffer.Resize(EditInfo.Size);

                        uint8_t* BufferPtr = EditBuffer.Data();

                        if (EditInfo.Size == 4) {
                            float Value = stof(it->second);
                            Value = IsBigEndian ? htobf(Value) : htolf(Value);
                            memcpy(BufferPtr, &Value, 4);
                        } else if (EditInfo.Size == 8) {
                            double Value = stod(it->second);
                            Value = IsBigEndian ? htobd(Value) : htold(Value);
                            memcpy(BufferPtr, &Value, 8);
                        } else {
                            it->second = "unsupported size: " + to_string(EditInfo.Size);
                            break;
                        }
                        it = Edits.erase(it);
                        Found = true;
                    } catch (const exception&) {
                        it->second = "invalid float value";
                    }
                    break;
                }
                }
                break;
            }
        }
        if (!Found)
            ++it;
    }
}

//---------------------------------------------------------------------------
string dpx::ListEditsParser()
{
    string Result = "DPX:\n";
    for (auto& EditInfo : EditList) {
        Result += "  " + string(EditInfo.Name);
        if (EditInfo.Description)
            Result += " (" + string(EditInfo.Description) + ")";
        Result += "\n";
    }
    return Result;
}

//---------------------------------------------------------------------------
void dpx::ParseBuffer()
{
    // Handle "same as the previous frame" content
    if (HeaderCopy)
    {
        // Size
        size_t HeaderCopy_Size = HeaderCopy_Info & 0xFFF;
        HeaderCopy_Size++;

        // Adapt previous frame content from new frame content
        uint32_t* HeaderCopy32 = (uint32_t*)HeaderCopy;
        const uint32_t* Buffer32 = (const uint32_t*)Buffer.Data();
        memmove(HeaderCopy + 36, Buffer.Data() + 36, 160 - 36); // Image filename + Creation date/time: yyyy:mm:dd:hh:mm:ssLTZ
        memmove(HeaderCopy + 1532, Buffer.Data() + 1532, 24); // Image filename + Creation date/time: yyyy:mm:dd:hh:mm:ssLTZ
        HeaderCopy32[1676 / 4] = Buffer32[1676 / 4]; // Count
        HeaderCopy32[1712 / 4] = Buffer32[1712 / 4]; // Frame position in sequence
        HeaderCopy32[1920 / 4] = Buffer32[1920 / 4]; // SMPTE time code
        HeaderCopy[1929] = Buffer[1929]; // Field number

        // Compare
        if (memcmp(HeaderCopy, Buffer.Data(), Buffer.Size() >= 2048 ? 2048 : Buffer.Size()))
            Invalid(invalid::DittoKey_NotSame);

        //TODO: no need to check again if the file is supported
    }

    dpx_tested Info;

    Buffer_Offset = 0;
    uint32_t MagicNumber = Get_B4();
    switch (MagicNumber)
    {
    case 0x58504453: // XPDS
        Info.Endianness = endianness::LE;
        IsBigEndian = false;
        break;
    case 0x53445058: // SDPX
        Info.Endianness = endianness::BE;
        IsBigEndian = true;
        break;
    default:
        Undecodable(undecodable::Header);
        return;
    }
    SetDetected();

    uint32_t OffsetToImageData = Get_X4();
    uint64_t VersionNumberBig = Get_B4();
    switch (VersionNumberBig)
    {
    case 0x00000000LL: // Not conform to spec but it exists and it does not hurt
    case 0x56312E30LL:
    case 0x56322E30LL:
    case 0x76312E30LL:
    case 0x76322E30LL:
        break;
    default:
        Undecodable(undecodable::VersionNumber);
        return;
    }

    Buffer_Offset = 28;
    uint32_t IndustryHeaderSize = Get_X4();
    if (IndustryHeaderSize == (uint32_t)-1)
        IndustryHeaderSize = 0;
    Buffer_Offset = 660;
    uint32_t Encryption = Get_X4();
    if (Encryption != (uint32_t)-1 && Encryption != 0) // One file found with Encryption of 0 but not encrypted, we accept it.
        Unsupported(unsupported::Encryption);
    Buffer_Offset = 768;
    uint16_t Orientation = Get_X2();
    if (Get_X2() != 1)
        Unsupported(unsupported::NumberOfElements);
    uint32_t Width = Get_X4();
    uint32_t Height = Get_X4();
    Buffer_Offset = 780;
    if (Get_X4() != 0)
        Unsupported(unsupported::DataSign);
    Buffer_Offset = 800;
    uint8_t Descriptor = Get_X1();
    switch (Descriptor)
    {
    case  6: Info.ColorSpace = colorspace::Y; break;
    case 50: Info.ColorSpace = colorspace::RGB; break;
    case 51: Info.ColorSpace = colorspace::RGBA; break;
    default: Info.ColorSpace = (decltype(Info.ColorSpace))-1;
    }
    Buffer_Offset = 803;
    Info.BitDepth = Get_X1();
    Info.Packing = (packing)Get_X2();
    uint16_t Encoding = Get_X2();
    if (Encoding)
        Unsupported(unsupported::Encoding);
    uint32_t OffsetToData = Get_X4();
    if (OffsetToData)
    {
        if (OffsetToData < 1664 || OffsetToData > Buffer.Size())
            Undecodable(undecodable::OffsetToData);
        if (OffsetToImageData != OffsetToData)
            Unsupported(unsupported::OffsetToImageData); // FFmpeg specific, it prioritizes OffsetToImageData over OffsetToData. TODO: remove this limitation when future internal encoder is used
    }
    else
        OffsetToData = OffsetToImageData;
    if (Get_X4() != 0)
        Unsupported(unsupported::EolPadding);
    bool IsAltern = Info.BitDepth == 10
        && Info.ColorSpace != colorspace::RGB
        && (!memcmp(Buffer.Data() + 160, "Lasergraphics Inc.", 18) // Creator
            || !memcmp(Buffer.Data() + 160, "DIAMANT-Film", 12) // Creator
            || !memcmp(Buffer.Data() + 1556, "Scanity", 7)); // Input device name

    if (IndustryHeaderSize && InputInfo)
    {
        Buffer_Offset = 1724;
        double FrameRate_Film = Get_XF4(); // Frame rate of original (frames/s) 
        Buffer_Offset = 1940;
        double FrameRate_Television = Get_XF4(); // Temporal sampling rate or frame rate (Hz)

        // Coherency of frame rate
        if (FrameRate_Film && FrameRate_Television && FrameRate_Film != FrameRate_Television)
            Unsupported(unsupported::FrameRate_Incoherent);

        // Availability of frame rate
        // We have lot of DPX files without frame rate info, using FFmpeg default (25 at the moment of writing)
        //if (!FrameRate_Film && !FrameRate_Television)
        //    Unsupported(unsupported::FrameRate_Unavailable);

        InputInfo->FrameRate = FrameRate_Film ? FrameRate_Film : FrameRate_Television;
    }

    // Supported?
    for (const auto& DPX_Tested_Item : DPX_Tested)
    {
        if (DPX_Tested_Item.Test == Info)
        {
            Flavor = (decltype(Flavor))(&DPX_Tested_Item - DPX_Tested);
            break;
        }
    }
    if (Flavor == (decltype(Flavor))-1)
    {
        for (const auto& DPX_Also_Item : DPX_Also)
        {
            if (DPX_Also_Item.Test == Info)
            {
                Flavor = (decltype(Flavor))DPX_Also_Item.Flavor;
                break;
            }
        }
    }
    if (Flavor == (decltype(Flavor))-1)
        Unsupported(unsupported::Flavor);
    if (Orientation == 2 && !(DPX_Tested[Flavor].Info.Flags & VFlip))
        Unsupported(unsupported::Orientation);
    if (IsAltern && !(DPX_Tested[Flavor].Info.Flags & Altern))
        Unsupported(unsupported::Altern);
    if (HasErrors())
        return;

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
    if (Info.BitDepth > 10)
        slice_x = slice_x * 3 / 2; // 1.5x more slices if 16-bit
    if (slice_x > Width / 2)
        slice_x = Width / 2;
    if (slice_x > Height / 2)
        slice_x = Height / 2;
    if (!slice_x)
        slice_x = 1;

    // Computing which slice count is suitable
    size_t Slice_Multiplier = PixelsPerBlock((flavor)Flavor);

    if (Slice_Multiplier > 1 && !(DPX_Tested[Flavor].Info.Flags & BlockSpan))
    {
        // Temporary limitation because the decoder does not support yet the merge of data from 2 slices in one DPX block
        for (; slice_x; slice_x--)
        {
            if (Width % (slice_x * Slice_Multiplier) == 0)
                break;
        }
        if (slice_x == 0)
        {
            Unsupported(unsupported::PixelBoundaries);
        }
    }
    slice_y = slice_x;

    // Computing OffsetAfterData
    size_t OffsetAfterData = OffsetToData;
    size_t ContentSize_Multiplier = BytesPerBlock((flavor)Flavor);
    if (MayHavePaddingBits((flavor)Flavor))
    {
        if (IsAltern)
        {
            auto BlockCountPerLine = (Width * Height + Slice_Multiplier - 1) / PixelsPerBlock((flavor)Flavor);
            OffsetAfterData += BlockCountPerLine * ContentSize_Multiplier;
        }
        else
        {
            auto BlockCountPerLine = (Width + Slice_Multiplier - 1) / PixelsPerBlock((flavor)Flavor);
            OffsetAfterData += BlockCountPerLine * ContentSize_Multiplier * Height;
        }
    }
    else
    {
        size_t BitsPerLine = Width * Colorspace2Count(DPX_Tested[(uint8_t)Flavor].Test.ColorSpace) * DPX_Tested[(uint8_t)Flavor].Test.BitDepth;
        auto WidthPadding = BitsPerLine % 32;
        if (WidthPadding)
            BitsPerLine += 32 - WidthPadding;
        OffsetAfterData += BitsPerLine / 8 * Height;
    }
    if (OffsetAfterData > Buffer.Size())
    {
        if (!Actions[Action_AcceptTruncated])
            Undecodable(undecodable::DataSize);
    }

    // Can we compress?
    if (!HasErrors())
        SetSupported();

    // Addition settings
    if (Orientation == 2)
        Flavor |= (uint64_t)1 << (int)feature::VFlip;
    if (IsAltern)
        Flavor |= (uint64_t)1 << (int)feature::Altern;

    // Testing padding bits
    if (IsSupported() && !Actions[Action_AcceptTruncated] && Actions[Action_CheckPadding] && RAWcooked)
    {
        bool HasPaddingBitsNotZero = false;
        size_t i, EOL_i;
        size_t Step, EOL_Step;
        uint32_t Mask, EOL_Mask;
        if (DPX_Tested[(uint8_t)Flavor].Test.Packing == packing::Packed)
        {
            auto UsedBits = Width * Info.BitDepth * Colorspace2Count(Info.ColorSpace);
            size_t RemainingPaddingBits = UsedBits % 32;
            if (RemainingPaddingBits)
            {
                // End of line
                size_t BytesPerLineMinus4 = (UsedBits / 32) * 4;
                i = EOL_i = OffsetToData + BytesPerLineMinus4;
                Step = EOL_Step = BytesPerLineMinus4 + 4;
                EOL_Mask = ((uint32_t)-1) << RemainingPaddingBits;
            }
            else
                i = OffsetAfterData; // No padding
        }
        else // Filled
        {
            // End of 32-bit packets
            bool IsFilledB = Info.Packing == packing::FilledB;
            i = OffsetToData;
            Step = Info.BitDepth == 10 ? 4 : 2;
            if (IsBigEndian ^ IsFilledB)
                i += Step - 1;
            Mask = Info.BitDepth == 10 ? 0x3 : 0xF;
            if (IsFilledB)
                Mask <<= Info.BitDepth == 10 ? 6 : 4;

            // End of line
            if ((flavor)Flavor == flavor::Raw_Y_10_FilledA_BE || (flavor)Flavor == flavor::Raw_Y_10_FilledB_BE)
            {
                size_t EOL_RemainingPaddingBits;
                if (IsAltern)
                    EOL_RemainingPaddingBits = (Width * Height) % 3;
                else
                    EOL_RemainingPaddingBits = Width % 3;

                if (EOL_RemainingPaddingBits)
                {
                    if (IsAltern)
                    {
                        EOL_i = OffsetAfterData - 4;
                        EOL_Step = 4;
                    }
                    else
                    {
                        size_t BytesPerLineMinus4 = (Width / 3) * 4;
                        EOL_i = OffsetToData + BytesPerLineMinus4;
                        EOL_Step = BytesPerLineMinus4 + 4;
                    }
                    EOL_RemainingPaddingBits *= 10;
                    if ((flavor)Flavor == flavor::Raw_Y_10_FilledA_BE)
                        EOL_RemainingPaddingBits += 2;
                    EOL_Mask = ((uint32_t)-1) << EOL_RemainingPaddingBits;
                    if ((flavor)Flavor == flavor::Raw_Y_10_FilledA_BE)
                        EOL_Mask |= 0x3;
                }
                else
                    EOL_i = OffsetAfterData; // No end of line padding
            }
            else
                EOL_i = OffsetAfterData; // No end of line padding
        }

        // Test
        for (; i < OffsetAfterData; i += Step)
        {
            if (i >= EOL_i)
            {
                if (ntoh(*((const uint32_t*)(Buffer.Data() + EOL_i))) & EOL_Mask)
                    break;
                EOL_i += EOL_Step;
            }
            else if (Buffer[i] & Mask)
                break;
        }
        if (i < OffsetAfterData)
        {
            // Non-zero padding bit found, storing data
            HasPaddingBitsNotZero = true;
            auto Temp_Size = OffsetAfterData - OffsetToData;
            if (Temp_Size > In.Size())  // Reuse old buffer if any and big enough
            {
                In.Create(Temp_Size);
                In_FirstNonZero = 0;
            }
            auto In_FirstNonZero_New = min(i, EOL_i);
            memset(In.Data() + In_FirstNonZero, 0x00, Temp_Size - In_FirstNonZero);
            In_FirstNonZero = In_FirstNonZero_New - OffsetToData;
            for (; i < OffsetAfterData; i += Step)
            {
                if (i >= EOL_i)
                {
                    *((uint32_t*)(In.Data() + EOL_i - OffsetToData)) = hton(ntoh(*((const uint32_t*)(Buffer.Data() + EOL_i))) & EOL_Mask);
                    EOL_i += EOL_Step;
                }
                else
                    In[i - OffsetToData] = Buffer[i] & Mask;
            }
        }
        if (!HasPaddingBitsNotZero)
            In.Clear();
    }

    // Write RAWcooked file
    if (IsSupported() && RAWcooked)
    {
        RAWcooked->Unique = false;
        RAWcooked->BeforeData = Buffer.Data();
        RAWcooked->BeforeData_Size = OffsetToData;
        RAWcooked->AfterData = Buffer.Data() + OffsetAfterData;
        RAWcooked->AfterData_Size = Buffer.Size() - OffsetAfterData;
        RAWcooked->InData = In.Data();
        RAWcooked->InData_Size = In.Size();
        RAWcooked->FileSize = (uint64_t)-1;
        if (Actions[Action_Hash])
        {
            Hash();
            RAWcooked->HashValue = &HashValue;
        }
        else
            RAWcooked->HashValue = nullptr;
        RAWcooked->IsAttachment = false;
        RAWcooked->Parse();
    }

    if (Actions[Action_Conch])
        ConformanceCheck();

    Edit();
}

//---------------------------------------------------------------------------
void dpx::BufferOverflow()
{
    Undecodable(undecodable::BufferOverflow);
}

//---------------------------------------------------------------------------
void dpx::ConformanceCheck()
{
    Buffer_Offset = 4;
    uint32_t OffsetToImageData = Get_X4();
    if (OffsetToImageData < 1664 || OffsetToImageData > Buffer.Size())
        Invalid(invalid::OffsetToImageData);
    uint64_t VersionNumberBig = Get_B4();
    switch (VersionNumberBig)
    {
    case 0x00000000LL:
    case 0x76312E30LL:
    case 0x76322E30LL:
        Invalid(invalid::VersionNumber);
    default:;
    }
    Buffer_Offset = 16;
    uint32_t TotalImageFileSize = Get_X4();
    if (TotalImageFileSize != FileSize)
        Invalid(invalid::TotalImageFileSize);
    uint32_t DittoKey = Get_X4();
    if (DittoKey > 1 && DittoKey != (uint32_t)-1)
        Invalid(invalid::DittoKey);
    Buffer_Offset = 770;
    uint16_t NumberOfElements = Get_X2();
    if (NumberOfElements == 0 || NumberOfElements > 8)
    {
        Invalid(invalid::NumberOfElements);
        if (NumberOfElements)
            NumberOfElements = 8; // File has an issue, testing only the first 8 elements
    }
    if (Buffer_Offset + 72 * NumberOfElements > Buffer.Size())
        NumberOfElements = (uint16_t)((Buffer.Size() - Buffer_Offset) / 72); // File has an issue, testing element which can fit in file size
    Buffer_Offset = 804;
    bool HasEncoding = false;
    for (uint16_t i = 0; i < NumberOfElements; i++)
    {
        uint16_t Packing = Get_X2();
        if (Packing > (uint16_t)packing::FilledB)
            Invalid(invalid::Packing);
        uint16_t Encoding = Get_X2();
        if (!HasEncoding && Encoding)
            HasEncoding = true;
        uint32_t OffsetToData = Get_X4();
        if (OffsetToData < 1664 || OffsetToData > Buffer.Size())
        {
            if (i) // if EOL_i == 0, already signaled in the common parsing
                Undecodable(undecodable::OffsetToData);
        }
        else if (OffsetToData < OffsetToImageData)
            Invalid(invalid::OffsetToImageData);
        Buffer_Offset += 68; // Next element
    }

    if (DittoKey == 0 && Buffer.Size() >= 1664)
    {
        // Copy header content so we compare content in next frames
        HeaderCopy_Info = OffsetToImageData;
        if (HeaderCopy_Info < 1664)
            HeaderCopy_Info = 1664; // Do not trust OffsetToImageData
        if (HeaderCopy_Info > 2048)
            HeaderCopy_Info = 2048; // Do not compare user data
        HeaderCopy = new uint8_t[2048];
        memmove(HeaderCopy, Buffer.Data(), HeaderCopy_Info >= 2048 ? 2048 : HeaderCopy_Info);
        HeaderCopy_Info--;
        HeaderCopy_Info |= (HasEncoding ? 1 : 0) << 12;
    }
}

//---------------------------------------------------------------------------
void dpx::Edit()
{
    for (const auto& Edit : Edits) {
        if (memcmp((const void*)(Buffer.Data() + Edit.first), Edit.second.Data(), Edit.second.Size())) {
            memcpy((void*)(Buffer.Data() + Edit.first), Edit.second.Data(), Edit.second.Size());
        }
    }
}

//---------------------------------------------------------------------------
string dpx::Flavor_String()
{
    return DPX_Flavor_String((uint8_t)Flavor);
}

//---------------------------------------------------------------------------
size_t dpx::BytesPerBlock(dpx::flavor Flavor)
{
    return DPX_Tested[(uint8_t)Flavor].Info.BytesPerBlock;
}

//---------------------------------------------------------------------------
size_t dpx::PixelsPerBlock(dpx::flavor Flavor)
{
    return DPX_Tested[(uint8_t)Flavor].Info.PixelsPerBlock;
}

//---------------------------------------------------------------------------
bool dpx::MayHavePaddingBits(dpx::flavor Flavor)
{
    if ((flavor)Flavor == (flavor)-1)
        return false;

    const auto& Info = DPX_Tested[(uint8_t)Flavor].Test;
    switch (Info.Packing)
    {
    case packing::FilledA:
    case packing::FilledB:
        break;
    default:
        return false;
    }

    return Info.BitDepth % 8;
}

//---------------------------------------------------------------------------
static const char* Packing_String(packing Packing)
{
    switch (Packing)
    {
    case packing::Packed : return "Packed";
    case packing::FilledA: return "FilledA";
    case packing::FilledB: return "FilledB";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
string DPX_Flavor_String(uint8_t Flavor)
{
    const auto& Info = DPX_Tested[(uint8_t)Flavor].Test;
    string ToReturn("DPX/");
    ToReturn += Raw_Flavor_String(Info.BitDepth, sign::U, Info.Endianness, Info.ColorSpace);
    if (Info.BitDepth % 8)
    {
        const char* Value = Packing_String(Info.Packing);
        if (Value)
        {
            ToReturn += '/';
            ToReturn += Value;
        }
    }
    return ToReturn;
}
