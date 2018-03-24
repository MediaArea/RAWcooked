/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FrameHash_MD5TH
#define FrameHash_MD5H
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Config.h"
using namespace std;
//---------------------------------------------------------------------------

class raw_frame;

class framehash_md5
{
public:
    framehash_md5();

    void To(raw_frame*          RawFrame);

private:
    ;
};

//---------------------------------------------------------------------------
#endif
