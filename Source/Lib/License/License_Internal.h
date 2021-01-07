/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
*
*  Use of this source code is governed by a BSD-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#ifndef License_ListH
#define License_ListH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include "Lib/Uncompressed/EXR/EXR.h"
#include "Lib/Uncompressed/WAV/WAV.h"
#include "Lib/Uncompressed/AIFF/AIFF.h"
#include "Lib/License/License.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Default license content
static const uint8_t License_Parser_Offset = 3;
struct default_license_values
{
    uint8_t                     Value;
    uint8_t                     Flavor;
};
static default_license_values DefaultLicense_Parsers[] =
{
    { 1                                      , (uint8_t)muxer::Matroska                          },
    { 2                                      , (uint8_t)encoder::FFV1                            },
    { 2                                      , (uint8_t)encoder::FLAC                            },
    { License_Parser_Offset + Parser_DPX     , (uint8_t)dpx::flavor::Raw_RGB_8                   },
    { License_Parser_Offset + Parser_DPX     , (uint8_t)dpx::flavor::Raw_RGB_10_FilledA_LE       },
    { License_Parser_Offset + Parser_DPX     , (uint8_t)dpx::flavor::Raw_RGB_10_FilledA_BE       },
    { License_Parser_Offset + Parser_WAV     , (uint8_t)wav::flavor::PCM_48000_16_2_LE           },
    { (uint8_t)-1                            , (uint8_t)-1                                       },
};

//---------------------------------------------------------------------------
struct feature_info
{
    const char*     Name;
};
const feature_info FeatureInfos[] =
{
    { "General options"         },
    { "Input options"           },
    { "Encoding options"        },
    { "More than 2 tracks"      },
};
static_assert(sizeof(FeatureInfos) / sizeof(feature_info) == feature_Max, "feature_info issue");

//---------------------------------------------------------------------------
struct muxer_info
{
    const char*     Name;
};
const muxer_info MuxerInfos[] =
{
    { "Matroska"    },
};
static_assert(sizeof(MuxerInfos) / sizeof(muxer_info) == muxer_Max, "muxer_info issue");

//---------------------------------------------------------------------------
struct encoder_info
{
    const char*     Name;
};
const encoder_info EncoderInfos[] =
{
    { "FFV1"        },
    { "PCM"         },
    { "FLAC"        },
};
static_assert(sizeof(EncoderInfos) / sizeof(encoder_info) == encoder_Max, "encoder_info issue");

//---------------------------------------------------------------------------
static string Features_String(uint8_t Value) { return FeatureInfos[Value].Name; }
static string Muxers_String(uint8_t Value) { return MuxerInfos[Value].Name; }
static string Encoders_String(uint8_t Value) { return EncoderInfos[Value].Name; }
typedef string (*flavor_string) (uint8_t Flavor);
struct license_info
{
    const char*     Name;
    uint8_t         Size;
    flavor_string   Flavor_String;
};
static const license_info License_Infos[] =
{
    { "features"            , feature_Max           , Features_String       },
    { "muxers/demuxers"     , muxer_Max             , Muxers_String         },
    { "encoders/decoders"   , encoder_Max           , Encoders_String       },
    { "DPX flavors"         , dpx::flavor_Max       , DPX_Flavor_String     },
    { "TIFF flavors"        , tiff::flavor_Max      , TIFF_Flavor_String    },
    { "EXR flavors"         , exr::flavor_Max       , EXR_Flavor_String     },
    { "WAV flavors"         , wav::flavor_Max       , WAV_Flavor_String     },
    { "AIFF flavors"        , aiff::flavor_Max      , AIFF_Flavor_String    },
};
const size_t License_Infos_Size = License_Parser_Offset + Uncompressed_Max;
static_assert(sizeof(License_Infos) / sizeof(license_info) == License_Infos_Size, "license_info issue");

//---------------------------------------------------------------------------
class license_internal
{
public:
                        license_internal(bool IgnoreDefault = false);

    time_t              Date = (time_t)-1;
    uint64_t            OwnerID = 0;

    uint8_t             Input_Flags[License_Parser_Offset] = {};

    // Tests
    void                SetSupported(uint8_t Type, uint8_t SubType);
    void                SetSupported(feature Flavor) { SetSupported(0, (uint8_t)Flavor); }
    void                SetSupported(muxer Flavor) { SetSupported(1, (uint8_t)Flavor); }
    void                SetSupported(encoder Flavor) { SetSupported(2, (uint8_t)Flavor); }
    void                SetSupported(parser Parser, uint8_t Flavor) { SetSupported(License_Parser_Offset + Parser, Flavor); }
    void                SetSupported(dpx::flavor Flavor) { SetSupported(Parser_DPX, (uint8_t)Flavor); }
    void                SetSupported(tiff::flavor Flavor) { SetSupported(Parser_TIFF, (uint8_t)Flavor); }
    void                SetSupported(exr::flavor Flavor) { SetSupported(Parser_EXR, (uint8_t)Flavor); }
    void                SetSupported(wav::flavor Flavor) { SetSupported(Parser_WAV, (uint8_t)Flavor); }
    void                SetSupported(aiff::flavor Flavor) { SetSupported(Parser_AIFF, (uint8_t)Flavor); }
    bool                IsSupported(uint8_t Type, uint8_t SubType);
    bool                IsSupported(parser Parser, uint8_t Flavor) { return IsSupported(License_Parser_Offset + Parser, Flavor); }

    // I/O
    buffer              ToBuffer();
    string              ToString();
    bool                FromBuffer(const buffer_view& Buffer);
    bool                FromString(const string& FromUser);

private:
    vector<bool>        Flags_;

    size_t              Flags_Pos_Get(uint8_t Type, uint8_t SubType);
};

#endif
