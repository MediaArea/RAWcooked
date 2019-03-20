/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef MatroskaH
#define MatroskaH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/FFV1/FFV1_Frame.h"
#include "Lib/Input_Base.h"
#include "Lib/FileIO.h"
#include <cstdint>
#include <string>
#include <vector>
#include <condition_variable>
using namespace std;
//---------------------------------------------------------------------------

namespace matroska_issue
{
    namespace undecodable { enum code : uint8_t; }
    namespace unsupported { enum code : uint8_t; }
}

class matroska;
class matroska_mapping;
class ThreadPool;
class flac_info;

class frame_writer
{
public:
    // Constructor / Destructor
    frame_writer(const string& BaseDirectory_Source, user_mode* UserMode_Soure, ask_callback Ask_Callback_Source, matroska* M_Source, errors* Errors_Source = nullptr) :
        BaseDirectory(BaseDirectory_Source),
        UserMode(UserMode_Soure),
        Ask_Callback(Ask_Callback_Source),
        M(M_Source),
        Errors(Errors_Source)
    {
    }

    // Config
    enum mode
    {
        IsNotBegin,
        IsNotEnd,
        mode_Max,
    };
    bitset<mode_Max>            Mode;

    // Actions
    void                        FrameCall(raw_frame* RawFrame, const string& OutputFileName);

private:
    bool                        Write(raw_frame* RawFrame, const string& OutputFileName);
    bool                        Check(raw_frame* RawFrame, const string& OutputFileName);
    file                        File_Write;
    filemap                     File_Read;
    string                      BaseDirectory;
    user_mode*                  UserMode;
    ask_callback                Ask_Callback;
    matroska*                   M;
    errors*                     Errors;
    size_t                      Offset;
    size_t                      SizeOnDisk;
};

class matroska : public input_base
{
public:
    matroska(const string& OutputDirectoryName, user_mode* Mode, ask_callback Ask_Callback, errors* Errors = nullptr);
    ~matroska();

    void                        Shutdown();

    bool                        Quiet;

    // Theading relating functions
    void                        ProgressIndicator_Show();
    void                        ProgressIndicator_Trigger();

    // libFLAC related helping functions
    void                        FLAC_Read(uint8_t buffer[], size_t* bytes);
    void                        FLAC_Tell(uint64_t* absolute_byte_offset);
    void                        FLAC_Metadata(uint8_t channels, uint8_t bits_per_sample);
    void                        FLAC_Write(const uint32_t* buffer[], size_t blocksize);

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
    void                        Undecodable(matroska_issue::undecodable::code Code) { input_base::Undecodable((error::undecodable::code)Code); }
    void                        Unsupported(matroska_issue::unsupported::code Code) { input_base::Unsupported((error::unsupported::code)Code); }

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
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_AfterData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_BeforeData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_InData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileSize);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionAfterData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionBeforeData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionFileName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_MaskAdditionInData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedSegment);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryVersion);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedSegment_PathSeparator);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_AfterData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_BeforeData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_FileName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryVersion);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_InData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseAfterData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseBeforeData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseFileName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_MaskBaseInData);
    MATROSKA_ELEMENT(Segment_Cluster);
    MATROSKA_ELEMENT(Segment_Cluster_SimpleBlock);
    MATROSKA_ELEMENT(Segment_Cluster_Timestamp);
    MATROSKA_ELEMENT(Segment_Tracks);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_CodecID);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_CodecPrivate);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_Video);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_Video_PixelWidth);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_Video_PixelHeight);
    MATROSKA_ELEMENT(Void);

    enum format
    {
        Format_None,
        Format_FFV1,
        Format_FLAC,
        Format_PCM,
        Format_Max,
    };

    string                      RAWcooked_LibraryName;
    string                      RAWcooked_LibraryVersion;
    struct trackinfo
    {
        frame_writer            FrameWriter;
        uint8_t*                Mask_FileName;
        uint8_t*                Mask_Before;
        uint8_t*                Mask_After;
        uint8_t*                Mask_In;
        uint8_t**               DPX_FileName;
        uint64_t*               DPX_FileSize;
        uint8_t**               DPX_Before;
        uint8_t**               DPX_After;
        uint8_t**               DPX_In;
        size_t                  Mask_FileName_Size;
        size_t                  Mask_Before_Size;
        size_t                  Mask_After_Size;
        size_t                  Mask_In_Size;
        size_t*                 DPX_FileName_Size;
        size_t*                 DPX_Before_Size;
        size_t*                 DPX_After_Size;
        size_t*                 DPX_In_Size;
        size_t                  DPX_Buffer_Pos;
        size_t                  DPX_Buffer_Count;
        raw_frame*              R_A;
        raw_frame*              R_B;
        flac_info*              FlacInfo;
        frame                   Frame;
        bool                    Unique;
        format                  Format;

        trackinfo(frame_writer& FrameWriter_Source) :
            FrameWriter(FrameWriter_Source),
            Mask_FileName(nullptr),
            Mask_Before(nullptr),
            Mask_After(nullptr),
            Mask_In(nullptr),
            DPX_FileName(nullptr),
            DPX_FileSize(nullptr),
            DPX_Before(nullptr),
            DPX_After(nullptr),
            DPX_In(nullptr),
            Mask_FileName_Size(0),
            Mask_Before_Size(0),
            Mask_After_Size(0),
            Mask_In_Size(0),
            DPX_FileName_Size(0),
            DPX_Before_Size(0),
            DPX_After_Size(0),
            DPX_In_Size(0),
            DPX_Buffer_Pos(0),
            DPX_Buffer_Count(0),
            R_A(nullptr),
            R_B(nullptr),
            FlacInfo(nullptr),
            Unique(false),
            Format(Format_None)
            {
            }
    };
    vector<trackinfo*>          TrackInfo;
    size_t                      TrackInfo_Pos;
    vector<uint8_t>             ID_to_TrackOrder;
    string                      AttachedFile_FileName;
    ThreadPool*                 FramesPool;
    frame_writer                FrameWriter_Template;
    condition_variable          ProgressIndicator_IsEnd;
    bool                        ProgressIndicator_IsPaused = false;
    uint64_t                    Cluster_Timestamp;
    int16_t                     Block_Timestamp;

    //Utils
    void Uncompress(uint8_t* &Output, size_t &Output_Size);
    void SanitizeFileName(uint8_t* &FileName, size_t &FileName_Size);
    void RejectIncompatibleVersions();
    void ProcessCodecPrivate_FFV1();
    void ProcessCodecPrivate_FLAC();
    void ProcessFrame_FLAC();
};

//---------------------------------------------------------------------------
#endif
