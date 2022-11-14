/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Utils/Errors/Errors.h"
#include "Lib/Utils/FileIO/FileChecker.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
namespace filechecker_issue {

    namespace undecodable
    {

        static const char* MessageText[] =
        {
            "files are not same",
            "missing attachments in compressed file",
            "extra attachments in compressed file",
            "missing attachments in source (extra attachments in compressed file)",
            "extra attachments in source (missing attachments in compressed file)",
            "missing frame in compressed file",
            "extra frame in compressed file",
            "missing frame in source (extra frames in compressed file)",
            "extra frame in source (missing frames in compressed file)",
            "track format (unsupported)",
        };

        namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

    } // unparsable

    const char** ErrorTexts[] =
    {
        undecodable::MessageText,
        nullptr,
        nullptr,
        nullptr,
    };

    static_assert(error::type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // filechecker_issue
