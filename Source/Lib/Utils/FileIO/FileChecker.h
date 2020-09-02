/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FileCheckerH
#define FileCheckerH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Utils/Errors/Errors.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
namespace filechecker_issue
{

    namespace undecodable
    {

        enum code : uint8_t
        {
            FileComparison,
            Attachment_Compressed_Missing,
            Attachment_Compressed_Extra,
            Attachment_Source_Missing,
            Attachment_Source_Extra,
            Frame_Compressed_Missing,
            Frame_Compressed_Extra,
            Frame_Source_Missing,
            Frame_Source_Extra,
            Max
        };

    } // unparsable

} // filechecker_issue

#endif