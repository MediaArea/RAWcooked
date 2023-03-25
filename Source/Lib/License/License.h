/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
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
#include <Lib/Utils/FileIO/Input_Base.h>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Features
ENUM_BEGIN(feature)
    GeneralOptions,
    InputOptions,
    EncodingOptions,
    MultipleTracks,
    SubLicense,
ENUM_END(feature)

//---------------------------------------------------------------------------
// Muxer
ENUM_BEGIN(muxer)
    Matroska,
ENUM_END(muxer)

//---------------------------------------------------------------------------
// Encoder
ENUM_BEGIN(encoder)
    FFV1,
    PCM,
    FLAC,
ENUM_END(encoder)

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
    bool ShowLicense(bool Verbose = false, uint64_t NewSublicenseId = 0, uint64_t NewSubLicenseDur = 0);

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
