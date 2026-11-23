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
#include <atomic>
#include <bitset>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
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
class block_pool;
struct output_hash;

class matroska : public input_base
{
public:
    matroska(const string& OutputDirectoryName, user_mode* Mode, ask_callback Ask_Callback, ThreadPool* Pool, errors* Errors = nullptr);
    ~matroska();

    void                        Shutdown(); // End of the outputs and checks, done only once (it may be called before the destructor for getting the results of the checks)

    bool                        Quiet = false;
    bool                        NoOutputCheck = false;
    std::function<bool()>       Decoders_Check; // Called when the format is detected, before any decoding, and before each block (a decoder may know later that it is not available e.g. a Vulkan device), parsing is stopped if it returns false
    bool                        Decoders_AreMissing = false;
    hashes*                     Hashes_FromRAWcooked = nullptr;
    hashes*                     Hashes_FromAttachments = nullptr;
    output_hash*                OutputHash = nullptr;
    vector<string>              Attachments_NotListed; // Attachments not in the reversibility data (e.g. added by another tool after the encoding), not checked, filled by Shutdown()

    // Theading relating functions
    void                        ProgressIndicator_Show();
    condition_variable          ProgressIndicator_IsEnd;
    bool                        ProgressIndicator_IsPaused = false;
    bool                        ProgressIndicator_Summary = true; // Speed shown at the end, else the progress indicator is erased
    bool                        ProgressIndicator_Shares = false; // Share of each decoder shown, if several decoders are used in parallel

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
    uint64_t                    Timestamp_Get() { return ProgressIndicator_Timestamp.load(std::memory_order_relaxed); } // Timestamp of the last parsed block, in ms

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
    bool Decoding_IsInit = false; // Init done at the first Cluster, else at the end of the parsing (no Cluster)
    bool Decoding_Init();

    // SeekHead, for finding the attachments after the clusters (e.g. added by another tool)
    size_t Segment_Offset = (size_t)-1; // Start of the Segment content, SeekHead positions are relative to it
    bool Attachments_IsParsed = false;
    bool SeekHead_IsParsed = false;
    bool SeekHead_HasSeekHead = false; // Another SeekHead is referenced, so the list of elements may be incomplete
    uint64_t SeekHead_Attachments = (uint64_t)-1; // Position of the attachments
    uint64_t Seek_ID = 0;
    uint64_t Seek_Position = (uint64_t)-1;
    void Seek_Set();

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
    MATROSKA_ELEMENT(Segment_SeekHead);
    MATROSKA_ELEMENT(Segment_SeekHead_Seek);
    MATROSKA_ELEMENT(Segment_SeekHead_Seek_SeekID);
    MATROSKA_ELEMENT(Segment_SeekHead_Seek_SeekPosition);
    MATROSKA_ELEMENT(Segment_Cluster);
    MATROSKA_ELEMENT(Segment_Cluster_BlockGroup);
    MATROSKA_ELEMENT(Segment_Cluster_BlockGroup_Block);
    MATROSKA_ELEMENT(Segment_Cluster_SimpleBlock);
    MATROSKA_ELEMENT(Segment_Cluster_Timestamp);
    MATROSKA_ELEMENT(Segment_Tracks);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_CodecID);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_CodecPrivate);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_TrackNumber);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_Video);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_Video_PixelWidth);
    MATROSKA_ELEMENT(Segment_Tracks_TrackEntry_Video_PixelHeight);
    MATROSKA_ELEMENT(Void);

    string                      RAWcooked_LibraryName;
    string                      RAWcooked_LibraryVersion;
    reversibility*              ReversibilityData = nullptr;
    vector<track_info*>         TrackInfo;
    size_t                      TrackInfo_Pos;
    track_info*                 Output_Track = nullptr; // Track with a possible frame output in another thread
    shared_ptr<block_pool>      Block_Pool; // Buffers for copies of frame data
    void                        Output_Wait();
    vector<uint64_t>            TrackInfo_Numbers; // TrackNumber declared by each TrackEntry (same index as TrackInfo), the position + 1 if not declared
    bool                        TrackNumbers_ArePositions = true; // All declared track numbers are the positions + 1 (fast path)
    size_t                      TrackNumber_Find(uint64_t TrackNumber) const; // Index in TrackInfo, or SIZE_MAX
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
    bool                        FileNames_AreLegacy = false; // Names in the ANSI code page of a previous version on Windows, detected from the names of attachments
    vector<string>              AttachedFile_FileNames_Empty; // Files with a size of 0 and a name not in UTF-8, created after the parsing of the names of all attachments
    enum reversibility_compat
    {
        Compat_Modern,
        Compat_18_10_1,
    };
    reversibility_compat        ReversibilityCompat = Compat_Modern;
    bool                        IsRejected = false; // Reversibility data is not supported, parsing is stopped and there is no final check
    ThreadPool*                 FramesPool = nullptr;
    frame_writer*               FrameWriter_Template;
    bool                        RAWcooked_FileNameIsValid;
    uint64_t                    Cluster_Timestamp;
    bool                        Shutdown_IsDone = false;

    // Progress indicator, values published by the parser for the progress indicator thread
    size_t                      ProgressIndicator_Total = 0; // Not modified during the life of the thread
    std::atomic<size_t>         ProgressIndicator_Offset{ 0 };
    std::atomic<uint64_t>       ProgressIndicator_Timestamp{ 0 };
    std::mutex                  ProgressIndicator_Mutex; // The end is notified under it, the thread tests the end under it before a wait (else it could start a wait after the notification)

    //Utils
    void                        ProcessBlock();
    void                        Uncompress(buffer& Buffer);
    void                        Segment_Attachments_AttachedFile_FileData_RawCookedxxx_yyy(reversibility::element Element, type Type);
    void                        StoreFromCurrentToEndOfElement(buffer& Output);
    void                        RejectIncompatibleVersions();
    void                        Reject(matroska_issue::undecodable::code Code);
    bool                        UnknownSize(uint64_t Name, uint64_t Size);
    void                        End();
    input_base_uncompressed_compound* InitOutput_Find();
    input_base_uncompressed_compound* InitOutput(input_base_uncompressed_compound* PotentialParser, raw_frame::flavor Flavor);
};

//---------------------------------------------------------------------------
#endif
