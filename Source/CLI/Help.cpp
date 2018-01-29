/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/Help.h"
#include "stdio.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
ReturnValue Help(const char* Name)
{
    printf("Usage: \"%s [options] FileNames [options]\"\n", Name);
    printf("\n");
    printf("Help:\n");
    printf("  --help, -h\n");
    printf("      Display this help and exit\n");

    return ReturnValue_OK;
}

//---------------------------------------------------------------------------
ReturnValue Usage(const char* Name)
{
    printf("Usage: \"%s [options] FileNames [options]\"\n", Name);
    printf("\"%s --help\" for displaying more information\n", Name);

    return ReturnValue_OK;
}
