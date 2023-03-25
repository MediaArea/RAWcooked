/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Common/Common.h"
using namespace std;
//---------------------------------------------------------------------------

//***************************************************************************
// Platform specific
//***************************************************************************

//---------------------------------------------------------------------------
void FormatPath(string& s)
{
    string::size_type i = 0;
    while ((i = s.find('\\', i)) != string::npos)
        s[i++] = '/';
}

//***************************************************************************
// Conversions
//***************************************************************************

//---------------------------------------------------------------------------
samplerate_code SampleRate2Code(uint32_t SampleRate)
{
    switch (SampleRate)
    {
    case 44100: return samplerate_code::_44100; break;
    case 48000: return samplerate_code::_48000; break;
    case 96000: return samplerate_code::_96000; break;
    default   : return (samplerate_code)-1;
    }
}

//---------------------------------------------------------------------------
size_t Colorspace2Count(colorspace ColorSpace)
{
    switch (ColorSpace)
    {
    case colorspace::RGB:
        return 3;
    case colorspace::RGBA:
        return 4;
    case colorspace::Y:
        return 1;
    default: return -1;
    }
}

//***************************************************************************
// Display
//***************************************************************************

//---------------------------------------------------------------------------
const char* Sign_String(sign Sign)
{
    switch (Sign)
    {
    case sign::S: return "S";
    case sign::U: return "U";
    case sign::F: return "F";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
const char* Endianess_String(endianness Endianness)
{
    switch (Endianness)
    {
    case endianness::LE: return "LE";
    case endianness::BE: return "BE";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
const char* ColorSpace_String(colorspace ColorSpace)
{
    switch (ColorSpace)
    {
    case colorspace::RGB: return "RGB";
    case colorspace::RGBA: return "RGBA";
    case colorspace::Y: return "Y";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
const char* SamplesPerSec_String(samplerate_code SampleRateCode)
{
    switch (SampleRateCode)
    {
    case samplerate_code::_44100: return "44";
    case samplerate_code::_48000: return "48";
    case samplerate_code::_96000: return "96";
    default: return nullptr;
    }
}

//---------------------------------------------------------------------------
string BitDepth_String(bitdepth BitDepth)
{
    return to_string((int)BitDepth);
}

//---------------------------------------------------------------------------
string Channels_String(channels Channels)
{
    return to_string((int)Channels);
}

//---------------------------------------------------------------------------
string Raw_Flavor_String(bitdepth BitDepth, sign Sign, endianness Endianness, colorspace ColorSpace)
{
    string ToReturn("Raw/");
    ToReturn += ColorSpace_String(ColorSpace);
    ToReturn += '/';
    ToReturn += BitDepth_String(BitDepth);
    ToReturn += "bit/";
    ToReturn += Sign_String(Sign);
    if (BitDepth >= 8)
    {
        ToReturn += '/';
        ToReturn += Endianess_String(Endianness);
    }
    return ToReturn;
}

//---------------------------------------------------------------------------
string PCM_Flavor_String(bitdepth BitDepth, sign Sign, endianness Endianness, channels Channels, samplerate_code SamplesPerSecCode)
{
    string ToReturn("PCM/");
    ToReturn += SamplesPerSec_String(SamplesPerSecCode);
    ToReturn += "kHz/";
    ToReturn += BitDepth_String(BitDepth);
    ToReturn += "bit/";
    ToReturn += Channels_String(Channels);
    ToReturn += "ch/";
    ToReturn += Sign_String(Sign);
    if (BitDepth >= 8)
    {
        ToReturn += '/';
        ToReturn += Endianess_String(Endianness);
    }
    return ToReturn;
}

