/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef TIFFH
#define TIFFH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/FFV1/FFV1_Frame.h"
#include "Lib/Input_Base.h"
#include <vector>
#include <set>
#include <cstdint>
#include <cstddef>
//---------------------------------------------------------------------------

namespace tiff_issue
{
    namespace undecodable { enum code : uint8_t; }
    namespace unsupported { enum code : uint8_t; }
}

class tiff : public input_base_uncompressed
{
public:
    tiff(errors* Errors = NULL);

    string                      Flavor_String();

    frame                       Frame;

    // Info
    size_t                      slice_x;
    size_t                      slice_y;

    enum flavor : uint8_t
    {
        Raw_RGB_8_U,
        Raw_RGB_16_U_BE,
        Raw_RGB_16_U_LE,
        Raw_RGBA_8_U,
        Raw_RGBA_16_U_LE,
        Flavor_Max,
    };

    enum endianess : uint8_t
    {
        LE,
        BE,
    };
    enum descriptor : uint32_t
    {
        Y_Invereted =   0,
        Y           =   1,
        RGB         =   2,
        Palette     =   3,
        A           =   4,
        CMYK        =   5,
        YCbCr       =   6,
        CIE_Lab     =   8,
        RGBA        =   2 + 0x10000, // Homemade for RGB + 1-component
        Descriptor_Max = (uint32_t)-1,
    };
    enum compression : uint16_t
    {
        Raw             =     1,
        CCITT_1D        =     2,
        CCITT_G3        =     3, // T4 in TIFF6
        CCITT_G4        =     4, // T6 in TIFF6
        LZW             =     5,
        JPEG            =     6,
        JPEG_Tech2      =     7,
        Deflate_Adobe   =     8,
        Next            = 32771, // NeXT 2-bit RLE
        CCITT_RleW      = 32771, // CCITT RLE
        Packbits        = 32773, // Mac RLE
        Thunderscan     = 32809, // Thunderscan RLE
        IT8CTPAD        = 32895, // IT8 CT with padding
        IT8LW           = 32896, // IT8 RLE
        IT8MP           = 32897, // IT8 Monochrome picture
        IT8BL           = 32898, // IT8 Binary Line
        PixarFilm       = 32908, // Pixar companded 10bit LZW
        PixarLog        = 32909, // Pixar companded 11bit ZIP
        DCS             = 32946, // Kodak DCS
        Deflate         = 32947,
        JBIG            = 34661, // ISO JPEG Big
        SgiLog          = 34676, // SGI Log Luminance RLE.
        SgiLog24        = 34677, // SGI Log 24-bit packed
        JPEG2000        = 34712,
        Compression_Max = (uint16_t)-1,
    };
    enum sampleformat : uint8_t
    {
        U       = 1,        // Unsigned
        S       = 2,        // Signed
        F       = 3,        // IEEE floating point
        Und     = 4,        // Undefined, to be handled as unsigned
        SampleFormat_Max,
    };

    // Info about formats
    static size_t BitsPerBlock(flavor Flavor);
    static size_t PixelsPerBlock(flavor Flavor); // Need no overlap every x pixels
    static descriptor Descriptor(flavor Flavor);
    static const char* Descriptor_String(flavor Flavor);
    static uint8_t BitsPerSample(flavor Flavor);
    static const char* BitsPerSample_String(flavor Flavor);
    static sampleformat SampleFormat(flavor Flavor);
    static const char* SampleFormat_String(flavor Flavor);
    static endianess Endianess(flavor Flavor);
    static const char* Endianess_String(flavor Flavor);

    // Specific parsing
    uint32_t Get_Element(std::vector<uint32_t>* List = NULL);
    uint32_t Get_ElementValue(uint32_t Count, uint32_t Size, std::vector<uint32_t>* List);
    struct data_content
    {
        size_t Begin;
        size_t End;

        bool operator < (const data_content& Value) const
        {
            return Begin < Value.Begin;
        }
    };
    std::set<data_content> DataContents;

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
    void                        Undecodable(tiff_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
    void                        Unsupported(tiff_issue::unsupported::code Code) { input_base::Unsupported((error::unsupported::code)Code); }
};

string TIFF_Flavor_String(uint8_t Flavor);

//---------------------------------------------------------------------------
#endif
