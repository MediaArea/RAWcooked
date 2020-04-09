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
#include "Lib/Uncompressed/WAV/WAV.h"
#include "Lib/Uncompressed/AIFF/AIFF.h"
#include "Lib/License/License.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Default license content
struct default_license_values
{
    uint8_t         Value;
    uint8_t         Flavor;
};
static default_license_values DefaultLicense_Parsers[] =
{
    { 1                 , (uint8_t)muxer::Matroska                          },
    { 2                 , (uint8_t)encoder::FFV1                            },
    { 2                 , (uint8_t)encoder::FLAC                            },
    { 3 + Parser_DPX    , (uint8_t)dpx::flavor::Raw_RGB_8                   },
    { 3 + Parser_DPX    , (uint8_t)dpx::flavor::Raw_RGB_10_FilledA_LE       },
    { 3 + Parser_DPX    , (uint8_t)dpx::flavor::Raw_RGB_10_FilledA_BE       },
    { 3 + Parser_WAV    , (uint8_t)wav::flavor::PCM_48000_16_2_LE           },
    { (uint8_t)-1       , (uint8_t)-1                                       },
};

//---------------------------------------------------------------------------
struct feature_info
{
    const char*     Name;
};
const feature_info FeatureInfos[feature_Max + 1] =
{
    { "General options"         },
    { "Input options"           },
    { "Encoding options"        },
    { "More than 2 tracks"      },
    { NULL                      },
};

//---------------------------------------------------------------------------
struct muxer_info
{
    const char*     Name;
};
const muxer_info MuxerInfos[muxer_Max] =
{
    { "Matroska"    },
};

//---------------------------------------------------------------------------
struct encoder_info
{
    const char*     Name;
};
const encoder_info EncoderInfos[encoder_Max] =
{
    { "FFV1"        },
    { "PCM"         },
    { "FLAC"        },
};

//---------------------------------------------------------------------------
static string Features_String(uint8_t Value) { return FeatureInfos[Value].Name; }
static string Muxers_String(uint8_t Value) { return MuxerInfos[Value].Name; }
static string Encoders_String(uint8_t Value) { return EncoderInfos[Value].Name; }
typedef string (*flavor_string) (uint8_t Flavor);
struct parser_info
{
    const char*     Name;
    uint8_t         Size;
    flavor_string   Flavor_String;
};
static const parser_info Infos[3 + Parser_Max + 1] =
{
    { "features"            , feature_Max           , Features_String       },
    { "muxers/demuxers"     , muxer_Max             , Muxers_String         },
    { "encoders/decoders"   , encoder_Max           , Encoders_String       },
    { "DPX flavors"         , dpx::flavor_Max       , DPX_Flavor_String     },
    { "TIFF flavors"        , tiff::flavor_Max      , TIFF_Flavor_String    },
    { "WAV flavors"         , wav::flavor_Max       , WAV_Flavor_String     },
    { "AIFF flavors"        , aiff::flavor_Max      , AIFF_Flavor_String    },
    { NULL                  , 0                     , NULL                  },
};

//---------------------------------------------------------------------------
struct license_internal
{
    time_t              Date;
    uint64_t            UserID;
    uint64_t            License_Flags[3 + Parser_Max];

    license_internal();

    static void         AddFlavor(uint64_t &Value, uint8_t Flavor);
    static const char*  SupportedFlavor(uint64_t &Value, uint8_t Flavor);

    // License info
    uint8_t             Input_Flags[3];
};

#endif
