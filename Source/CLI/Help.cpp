/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/Help.h"
#include "stdio.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
extern const char* LibraryName;
extern const char* LibraryVersion;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
ReturnValue Help(const char* Name)
{
    printf("Usage: \"%s [options] DirectoryName [options]\"\n", Name);
    printf("Alternative usage: \"%s [options] FileNames [options]\"\n", Name);
    printf("\n");
    printf("Help:\n");
    printf("  --help, -h\n");
    printf("      Display this help and exit\n");
    printf("  --version\n");
    printf("      Display version and exit\n");

    return ReturnValue_OK;
}

//---------------------------------------------------------------------------
ReturnValue Usage(const char* Name)
{
    printf("Usage: \"%s [options] DirectoryName [options]\"\n", Name);
    printf("\"%s --help\" for displaying more information\n", Name);
    printf("or \"man %s\" for displaying the man page\n", Name);

    return ReturnValue_OK;
}

//---------------------------------------------------------------------------
ReturnValue Version()
{
    printf("%s %s\n", LibraryName, LibraryVersion);

    return ReturnValue_OK;
}
