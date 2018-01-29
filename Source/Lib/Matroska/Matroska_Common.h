/*  Copyright (c) MediaArea.net SARL. All Rights Reserved.
 *
 *  Use of this source code is governed by a MIT-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef MatroskaH
#define MatroskaH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/FFV1/FFV1_Frame.h"
#include <cstdint>
#include <string>
//---------------------------------------------------------------------------

class matroska_mapping;
class ThreadPool;

typedef void (*write_frame_call)(uint64_t, raw_frame*, void*);

class matroska
{
public:
    matroska();
    ~matroska();

    uint8_t*                    Buffer;
    uint64_t                    Buffer_Size;

    void                        Parse();

    frame                       Frame;

    write_frame_call            WriteFrameCall;
    void*                       WriteFrameCall_Opaque;

    // Info
    bool                        IsDetected;

private:
    uint64_t                    Buffer_Offset;

    typedef void (matroska::*call)();
    typedef call(matroska::*name)(uint64_t);

    static const size_t Levels_Max = 16;
    struct levels_struct
    {
        name SubElements;
        uint64_t Offset_End;
    };
    levels_struct Levels[Levels_Max];
    size_t Level;
    bool IsList;

    #define MATROSKA_ELEMENT(_NAME) \
        void _NAME(); \
        call SubElements_##_NAME(uint64_t Name);
    
    MATROSKA_ELEMENT(_);
    MATROSKA_ELEMENT(Segment);
    MATROSKA_ELEMENT(Segment_Attachments);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_BeforeData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryVersion);
    MATROSKA_ELEMENT(Segment_Cluster);
    MATROSKA_ELEMENT(Segment_Cluster_SimpleBlock);
    MATROSKA_ELEMENT(Segment_Tracks);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_CodecPrivate);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_Video);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_Video_PixelWidth);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_Video_PixelHeight);
    MATROSKA_ELEMENT(Void);

    bool                        RAWCooked_LibraryName_OK;
    bool                        RAWCooked_LibraryVersion_OK;
    uint8_t**                   DPX_Buffer;
    uint64_t*                   DPX_Buffer_Size;
    string*                     DPX_Buffer_Name;
    size_t                      DPX_Buffer_Pos;
    size_t                      DPX_Buffer_Count;
    raw_frame*                  R_A;
    raw_frame*                  R_B;
    ThreadPool*                 FramesPool;
};

//---------------------------------------------------------------------------
#endif
