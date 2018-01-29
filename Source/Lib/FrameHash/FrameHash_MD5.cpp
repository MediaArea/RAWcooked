/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/FrameHash/FrameHash_MD5.h"
#include "Lib/RawFrame/RawFrame.h"
extern "C"
{
#include "Lib/ThirdParty/md5/md5.h"
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
framehash_md5::framehash_md5()
{
}

//---------------------------------------------------------------------------
void framehash_md5::To(raw_frame* RawFrame)
{
    MD5_CTX MD5;
    MD5_Init(&MD5);
    
    for (size_t p=0; p<RawFrame->Planes.size(); p++)
    {
        uint8_t* FrameBuffer_Temp= RawFrame->Planes[p]->Buffer;
        for (size_t h=0; h<RawFrame->Planes[p]->Height; h++)
        {
            MD5_Update(&MD5, FrameBuffer_Temp, (int)RawFrame->Planes[p]->ValidBytesPerLine());
            FrameBuffer_Temp+= RawFrame->Planes[p]->AllBytesPerLine();
        }
    }

    unsigned char Digest[16];
    MD5_Final(Digest, &MD5);
}
