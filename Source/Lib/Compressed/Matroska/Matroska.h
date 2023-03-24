/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef MatroskaH
#define MatroskaH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Compressed/RAWcooked/Reversibility.h"


#include "Lib/CoDec/FFV1/FFV1_Frame.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#include "Lib/Utils/FileIO/FileIO.h"
#include <bitset>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
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
class hashes;
class track_info;
class frame_writer;

class matroska : public input_base
{
public:
    matroska(const string& OutputDirectoryName, user_mode* Mode, ask_callback Ask_Callback, ThreadPool* Pool, errors* Errors = nullptr);
    ~matroska();

    void                        Shutdown();

    bool                        Quiet = false;
    bool                        NoOutputCheck = false;
    hashes*                     Hashes_FromRAWcooked = nullptr;
    hashes*                     Hashes_FromAttachments = nullptr;

    // Theading relating functions
    void                        ProgressIndicator_Show();
    condition_variable          ProgressIndicator_IsEnd;
    bool                        ProgressIndicator_IsPaused = false;

    // Enums
    enum class type
    {
        Block_,
        Block_MaskAddition,
        Track_,
        Track_MaskBase,
        Segment_,
    };

    // Info
    string                      RAWcooked_LibraryNameVersion_Get() { if (RAWcooked_LibraryName.empty()) return string(); return RAWcooked_LibraryName + ' ' + RAWcooked_LibraryVersion; }

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

    levels_struct Cluster_Levels[Levels_Max];
    size_t Cluster_Level = (size_t)-1;
    size_t Cluster_Offset = (size_t)-1;
    size_t Element_Begin_Offset;

    #define MATROSKA_ELEMENT(_NAME) \
        void _NAME(); \
        call SubElements_##_NAME(uint64_t Name);

    #define MATROSKA_ELEM_XY(_NAME, _X, _Y) \
        void _NAME##_X##_Y() { Segment_Attachments_AttachedFile_FileData_RawCookedxxx_yyy(reversibility::element::_Y, type::_X); } \
        call SubElements_##_NAME##_X##_Y(uint64_t Name);

    MATROSKA_ELEMENT(_);
    MATROSKA_ELEMENT(Segment);
    MATROSKA_ELEMENT(Segment_Attachments);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedAttachment);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileHash);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_FileSize);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedAttachment_InData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Block_, FileName);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Block_, AfterData);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Block_, BeforeData);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Block_, InData);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Block_MaskAddition, FileName);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Block_MaskAddition, AfterData);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Block_MaskAddition, BeforeData);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Block_MaskAddition, InData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileHash);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedBlock_FileSize);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedSegment);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedSegment_LibraryVersion);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedSegment_PathSeparator);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedSegment_FileHash);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedSegment_FileSize);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Segment_, FileName);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Segment_, InData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Track_, FileName);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Track_, AfterData);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Track_, BeforeData);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Track_, InData);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Track_MaskBase, AfterData);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Track_MaskBase, BeforeData);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Track_MaskBase, FileName);
    MATROSKA_ELEM_XY(Segment_Attachments_AttachedFile_FileData_RawCooked, Track_MaskBase, InData);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_FileHash);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryName);
    MATROSKA_ELEMENT(Segment_Attachments_AttachedFile_FileData_RawCookedTrack_LibraryVersion);
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

    string                      RAWcooked_LibraryName;
    string                      RAWcooked_LibraryVersion;
    reversibility*              ReversibilityData = nullptr;
    vector<track_info*>         TrackInfo;
    size_t                      TrackInfo_Pos;
    vector<uint8_t>             ID_to_TrackOrder;
    string                      AttachedFile_FileName;
    enum flags
    {
        IsInFromAttachments, // Has both name and size
        ReversibilityFileHasName,
        ReversibilityFileHasSize,
        Flags_Max
    };
    struct attached_file
    {
        uint64_t                FileSizeFromReversibilityFile = 0;
        uint64_t                FileSizeFromAttachments = 0;
        bitset<Flags_Max>       Flags;
    };
    map<string, attached_file>  AttachedFiles;
    set<string>                 AttachedFile_FileNames_IsHash; // Store the files detected as being hash file
    enum reversibility_compat
    {
        Compat_Modern,
        Compat_18_10_1,
    };
    reversibility_compat        ReversibilityCompat = Compat_Modern;
    ThreadPool*                 FramesPool = nullptr;
    frame_writer*               FrameWriter_Template;
    bool                        RAWcooked_FileNameIsValid;
    uint64_t                    Cluster_Timestamp;
    int16_t                     Block_Timestamp;

    //Utils
    void                        Uncompress(buffer& Buffer);
    void                        Segment_Attachments_AttachedFile_FileData_RawCookedxxx_yyy(reversibility::element Element, type Type);
    void                        StoreFromCurrentToEndOfElement(buffer& Output);
    void                        RejectIncompatibleVersions();
    bool                        UnknownSize(uint64_t Name, uint64_t Size);
    void                        End();
    input_base_uncompressed_compound* InitOutput_Find();
    input_base_uncompressed_compound* InitOutput(input_base_uncompressed_compound* PotentialParser, raw_frame::flavor Flavor);
};

//---------------------------------------------------------------------------
#endif
