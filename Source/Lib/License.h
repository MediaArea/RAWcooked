/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
*
*  Use of this source code is governed by a BSD-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#ifndef LicenseH
#define LicenseH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include <cstdint>
#include <string>
#include <Lib/Input_Base.h>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Features
enum feature : uint8_t
{
    Feature_GeneralOptions,
    Feature_InputOptions,
    Feature_EncodingOptions,
    Feature_MultipleTracks,
    Feature_Max,
};

//---------------------------------------------------------------------------
// Muxer
enum muxer : uint8_t
{
    Muxer_Matroska,
    Muxer_Max,
};

//---------------------------------------------------------------------------
// Encoder
enum encoder : uint8_t
{
    Encoder_FFV1,
    Encoder_PCM,
    Encoder_FLAC,
    Encoder_Max,
};

//---------------------------------------------------------------------------
struct license
{
public:
    // Constructor/Destructor
    license();
    ~license();

    // Set license info
    void Feature(feature Value);
    void Muxer(muxer Value);
    void Encoder(encoder Value);

    // License management
    bool LoadLicense(string LicenseKey, bool StoreLicenseKey);
    void ShowLicense(bool Verbose=false);

    // Checks
    bool IsSupported();
    bool IsSupported(feature Feature);
    bool IsSupported(parser Parser, uint8_t Flavor);
    bool IsSupported_License();

private:
    // Internal
    void*       Internal;
};

#endif
