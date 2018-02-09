/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef RawFrameH
#define RawFrameH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Config.h"
#include <cstring>
#include <vector>
using namespace std;
//---------------------------------------------------------------------------


class raw_frame
{
public:
    uint64_t                    Style_Private; //Used by specialized style for marking the configuration of such style (e.g. endianess of DPX)
    uint8_t*                    Pre;
    size_t                      Pre_Size;
    uint8_t*                    Post;
    size_t                      Post_Size;

    struct plane
    {
        uint8_t*                Buffer;
        size_t                  Buffer_Size;
        size_t                  Width;
        size_t                  Width_Padding;
        size_t                  Height;
        size_t                  BitsPerPixel;

        plane(size_t Width_, size_t Height_, size_t BitsPerPixel_)
            :
            Width(Width_),
            Height(Height_),
            BitsPerPixel(BitsPerPixel_)
        {
            Width_Padding=0; //TODO: option for padding size
            if (Width_Padding)
                Width_Padding-=Width%Width_Padding;
                
            Buffer_Size=(Width+Width_Padding)*Height*BitsPerPixel/8;
            Buffer=new uint8_t[Buffer_Size];
            memset(Buffer, 0, Buffer_Size);
        }

        ~plane()
        {
            delete[] Buffer;
        }

        size_t ValidBytesPerLine()
        {
            return Width*BitsPerPixel/8;
        }

        size_t AllBytesPerLine()
        {
            return (Width+Width_Padding)*BitsPerPixel/8;
        }
    };
    std::vector<plane*> Planes;

    enum style
    {
        Style_FFmpeg,
        Style_DPX,
    };
    style                       Style;

    raw_frame() :
        Style_Private(0),
        Pre(NULL),
        Post(NULL)
    {
    }
    
    ~raw_frame()
    {
        for (size_t i = 0; i < Planes.size(); i++)
            delete Planes[i];
    }

    // Creation
    void Create(style Style, size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample);

private:
    void FFmpeg_Create(size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample);
    void DPX_Create(size_t colorspace_type, size_t width, size_t height, size_t bits_per_raw_sample, bool chroma_planes, bool alpha_plane, size_t h_chroma_subsample, size_t v_chroma_subsample);
};

//---------------------------------------------------------------------------
#endif
