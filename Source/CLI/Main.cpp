/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>
#include <thread>
#include "CLI/Global.h"
#include "CLI/Input.h"
#include "CLI/Output.h"
#include "CLI/Help.h"
#include "Lib/Compressed/Matroska/Matroska.h"
#include "Lib/Uncompressed/DPX/DPX.h"
#include "Lib/Uncompressed/TIFF/TIFF.h"
#include "Lib/Uncompressed/EXR/EXR.h"
#include "Lib/Uncompressed/WAV/WAV.h"
#include "Lib/Uncompressed/AIFF/AIFF.h"
#include "Lib/Uncompressed/AVI/AVI.h"
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "Lib/Compressed/RAWcooked/RAWcooked.h"
#include "Lib/CoDec/FFmpeg_Library.h"
#include "Lib/CoDec/Wrapper.h"
#include "Lib/ThirdParty/thread-pool/include/ThreadPool.h"
#include "Lib/Utils/Trace/Trace.h"
#include <atomic>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
global Global;
input Input;
output Output;
rawcooked RAWcooked;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Stats (--show-stats)
static mutex Stats_Mutex;
static bool Stats_Vulkan_IsSelected = false;
static bool Stats_Vulkan_IsGPU = false;
static string Stats_Vulkan_Name; // Vulkan device used by the decoder

//---------------------------------------------------------------------------
// FFmpeg decoder with Vulkan selected, the device is shown once
static void Stats_VulkanSelected(const string& DeviceName, bool IsGPU)
{
    lock_guard<mutex> Lock(Stats_Mutex);
    if (Stats_Vulkan_IsSelected)
        return;
    Stats_Vulkan_IsSelected = true;
    Stats_Vulkan_IsGPU = IsGPU;
    Stats_Vulkan_Name = DeviceName.empty() ? string("unknown device") : DeviceName;
    cerr << "                                                            \r"; // Clean up the progress indicator
    cerr << "Info: " << (IsGPU ? "GPU" : "Vulkan CPU implementation") << " detected and selected for decoding: " << Stats_Vulkan_Name << ".\n" << endl;
}

//---------------------------------------------------------------------------
// Speed compared to the duration of the content, else frames per second if the duration of the content is unknown, else duration
static string Stats_Speed(size_t Frames, double Content_Seconds, double Seconds)
{
    ostringstream Result;
    Result.setf(ios::fixed, ios::floatfield);
    if (Content_Seconds > 0 && Seconds > 0)
    {
        auto RealTime = Content_Seconds / Seconds;
        Result << setprecision(RealTime < 0.1 ? 3 : 2) << RealTime << "x realtime";
    }
    else if (Frames && Seconds > 0)
        Result << setprecision(1) << Frames / Seconds << " fps";
    else
        Result << setprecision(2) << Seconds << " s";
    return Result.str();
}

//---------------------------------------------------------------------------
static string Stats_CPU()
{
    return "\n      CPU: " + global::CPU_Name() + '.';
}

//---------------------------------------------------------------------------
// Ask user about overwriting files
user_mode Ask_Callback(user_mode* Mode, const string& FileName, const string& ExtraText, bool Always, bool* ProgressIndicator_IsPaused, condition_variable* ProgressIndicator_IsEnd)
{
    if (Mode && *Mode != Ask)
        return *Mode;

    string Result;
    if (ProgressIndicator_IsPaused)
        *ProgressIndicator_IsPaused = true;
    if (ProgressIndicator_IsEnd)
        ProgressIndicator_IsEnd->notify_one();
    cerr << "                                                            \r"; // Clean up output
    cerr << "File '" << FileName << "' already exists" << ExtraText << ". Overwrite? [y/N] ";
    getline(cin, Result);
    if (ProgressIndicator_IsPaused)
        *ProgressIndicator_IsPaused = false;
    if (ProgressIndicator_IsEnd)
        ProgressIndicator_IsEnd->notify_one();
    cerr << "                                                            \r"; // Clean up output
    user_mode NewMode = (!Result.empty() && (Result[0] == 'Y' || Result[0] == 'y')) ? AlwaysYes : AlwaysNo;

    if (Mode && Always)
    {
        cerr << "Use this choice for all other files? [y/N] ";
        getline(cin, Result);
        if (!Result.empty() && (Result[0] == 'Y' || Result[0] == 'y'))
            * Mode = NewMode;
    }

    cerr << endl;
    return NewMode;
}

//---------------------------------------------------------------------------
static input_base_uncompressed* CreateParser(int i, errors* Errors, const input_base_uncompressed* SourceParser = nullptr)
{
    input_base_uncompressed* Parser;
    switch (i) {
    case Parser_DPX: Parser = new dpx(Errors); break;
    case Parser_TIFF: Parser = new tiff(Errors); break;
    case Parser_EXR: Parser = new exr(Errors); break;
    case Parser_WAV: Parser = new wav(Errors); break;
    case Parser_AIFF: Parser = new aiff(Errors); break;
    case Parser_AVI: Parser = new avi(Errors); break;
    default: return  (input_base_uncompressed*)nullptr;
    }

    if (SourceParser) {
        Parser->CopyCommon(*SourceParser);
    }

    return Parser;
}

int Help_Edit() {
    for (int i = 0; i < Uncompressed_Max; i++) {
        auto Parser = CreateParser(i, &Global.Errors);
        if (!Parser)
            continue;
        Parser->ListEdits();
        delete Parser;
    }

    return ReturnValue_OK;
}

//---------------------------------------------------------------------------
struct parse_info
{
    string* Name = {};
    filemap FileMap;
    vector<FileEntry> RemovedFiles;
    string FileName_Template;
    string FileName_StartNumber;
    string FileName_EndNumber;
    string FileList;
    string Flavor;
    string Slices;
    input_info InputInfo;
    size_t StreamCountMinus1 = 0;
    size_t FrameCount = 0; // Video frames in a container
    double FrameRate = 0; // Of the video in a container, 0 if unknown
    bool   IsDetected = false;
    bool   IsContainer = false;
    bool   Problem = false;

    bool ParseFile_Input_Uncompressed(input_base_uncompressed& SingleFile, input& Input, size_t Files_Pos);
};

//---------------------------------------------------------------------------
bool ParseFile_Input(input_base& SingleFile, filemap& FileMap, input_info* InputInfo, bool OverrideCheckPadding = false)
{
    // Init
    SingleFile.Actions = Global.Actions;
    if (OverrideCheckPadding)
        SingleFile.Actions.set(Action_CheckPadding);
    SingleFile.Hashes = Global.Hashes;
    SingleFile.InputInfo = InputInfo;

    // Parse
    SingleFile.Parse(FileMap);
    Global.ProgressIndicator_Increment();

    if (SingleFile.ParserCode == Parser_EXR && SingleFile.IsDetected() && !Global.Actions[Action_Check])
    {
        Global.ProgressIndicator_Stop();
        cerr << "EXR support depends a lot on the FFmpeg version you have, it is safer to double check the output,\n"
            "use --check for adding checks after the encoding." << endl;
        return true;
    }

    if (Global.Errors.HasErrors())
    {
        if (strstr(Global.Errors.ErrorMessage(), "becoming too big"))
        {
            Global.ProgressIndicator_Stop();
            cerr << "Error: the reversibility file is becoming big.\n"
                "       FFmpeg, used for muxing the output, has some issues with big\n"
                "       attachments, and such big reversibility file is not expected\n"
                "       with such compression, we prefer to be safe and we reject the\n"
                "       compression.\n"
                "       Use \"--output-version 2\" option for supporting\n"
                "       such big reversibility file. Decoding will not be possible\n"
                "       with RAWcooked older than version 21.09.\n"
                << endl;
        }
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
bool ParseFile_AdditionalInput(input_base_uncompressed& S, filemap& FileMap, const vector<FileEntry>& RemovedFiles, bool OverrideCheckPadding, size_t HeaderSize, const directory* Dir, size_t i)
{
    const auto& Name = RemovedFiles[i].FileName;
    if (input::OpenInput(FileMap, Name, RemovedFiles[i].FileSize, &Global.Errors, Global.Edits_Enabled, HeaderSize, Dir)) {
        return true;
    }
    if (Global.Actions[Action_Encode] || Global.Actions[Action_Hash]) { // Also needed for the hash, for its comparison with the hash files
        S.FileName = Name.substr(Global.Path_Pos_Global);
        FormatPath(S.FileName);
    }

    S.Index = i;
    if (ParseFile_Input(S, FileMap, nullptr, OverrideCheckPadding)) {
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------
struct worker_data {
    thread Thread;
    filemap FileMap;
    input_base_uncompressed* Parser;
};
bool ParseFile_AdditionalInputs(input_base_uncompressed& SingleFile, filemap& FileMap, const vector<FileEntry>& RemovedFiles, bool OverrideCheckPadding)
{
    // Edit only: only the beginning of the files is needed (hash and padding bits check read the whole files sequentially)
    auto EditOnly = Global.Edits_Enabled && !Global.Actions[Action_Encode] && !Global.Actions[Action_Hash] && !Global.Actions[Action_Conch] && !Global.Actions[Action_CheckPadding] && !OverrideCheckPadding;

    // Only the header is needed if the files are only edited, read and written without memory map (quicker on Linux and macOS, munmap() is costly)
    // Not on Windows: with Windows Defender, the antivirus work is doubled with ReadFile() and WriteFile(), the memory map is quicker
    #if defined(_WIN32) || defined(_WINDOWS)
    size_t HeaderSize = 0;
    #else //defined(_WIN32) || defined(_WINDOWS)
    size_t HeaderSize = EditOnly ? SingleFile.HeaderSize() : 0;
    #endif //defined(_WIN32) || defined(_WINDOWS)

    // Hash: the whole files are read once, read in a buffer without memory map (and without the file cache if possible)
    // Also when encoding (the encoding is done in another phase), else page faults in the threads of the hash would be a bottleneck
    if (!Global.Edits_Enabled && Global.Actions[Action_Hash])
    {
        HeaderSize = filemap::WholeFile;
        filemap::WholeFile_Max_Set(Global.IoThreads); // Max size of the files read in a buffer, from the available memory
    }

    // Edit only: files opened from their name relative to their directory (kept open), with random access
    // (no gain measured on local storage, less work expected on network storage)
    directory Dir_Temp;
    const directory* Dir = nullptr;
    if (EditOnly && RemovedFiles.size() > 1 && !Dir_Temp.Open_FromFileName(RemovedFiles[1].FileName))
        Dir = &Dir_Temp;

    if (Global.IoThreads > 1) {
        const size_t numFiles = RemovedFiles.size();
        atomic<size_t> nextIndex{ 1 };
        atomic<bool> AnyError{ false };
        atomic<bool> HashFlush{ false };
        auto Work = [&nextIndex, numFiles, &RemovedFiles, OverrideCheckPadding, HeaderSize, Dir, &AnyError, &HashFlush](input_base_uncompressed& Parser, filemap& Worker_FileMap) {
            for (;;) {
                auto i = nextIndex.fetch_add(1, memory_order_relaxed);
                if (i >= numFiles) {
                    // No more files, hashes of the last files computed without waiting for other files
                    if (Parser.Hash_Batch && !HashFlush.exchange(true))
                        input_base::Hash_Batch_Flush(true);
                    break;
                }
                if (ParseFile_AdditionalInput(Parser, Worker_FileMap, RemovedFiles, OverrideCheckPadding, HeaderSize, Dir, i)) {
                    AnyError = true;
                }
            }
        };
        vector<worker_data> Pool;
        Pool.resize(Global.IoThreads);
        Pool[0].Parser = &SingleFile;
        SingleFile.Hash_Batch = Global.Actions[Action_Hash] ? input_base::Hash_Batch_Count(Global.IoThreads) : 0; // Hash of several files in parallel
        for (size_t w = 1; w < Global.IoThreads; ++w) {
            // Initialize each worker
            Pool[w].Parser = CreateParser(SingleFile.ParserCode, &Global.Errors, &SingleFile);
            if (!Pool[w].Parser) {
                AnyError = true;
                continue;
            }
            Pool[w].Parser->Hash_Batch = SingleFile.Hash_Batch;

            // Launch each worker
            worker_data* p = &Pool[w];
            p->Thread = thread([p, &Work]() { Work(*p->Parser, p->FileMap); });
        }

        // The current thread is also a worker, with the first parser
        Work(SingleFile, FileMap);

        // Join threads and cleanup parsers
        for (size_t w = 0; w < Global.IoThreads; ++w) {
            auto& P = Pool[w];
            if (P.Thread.joinable())
                P.Thread.join();
            if (w)
                delete P.Parser;
        }
        if (HashFlush)
            input_base::Hash_Batch_Flush(false);
        SingleFile.Hash_Batch = 0;

        return AnyError.load();
    }

    for (size_t i = 1; i < RemovedFiles.size(); i++) {
        if (ParseFile_AdditionalInput(SingleFile, FileMap, RemovedFiles, OverrideCheckPadding, HeaderSize, Dir, i)) {
            return true;
        }
    }
    return false;
}

//---------------------------------------------------------------------------
bool parse_info::ParseFile_Input_Uncompressed(input_base_uncompressed& SingleFile, input& Input, size_t Files_Pos)
{
    if (SingleFile.IsDetected())
        return false;

    // Init
    RAWcooked.Mode = &Global.Mode;
    RAWcooked.Ask_Callback = Ask_Callback;
    RAWcooked.ProgressIndicator_IsEnd = &Global.ProgressIndicator_IsEnd;
    RAWcooked.ProgressIndicator_IsPaused = &Global.ProgressIndicator_IsPaused;
    RAWcooked.Errors = &Global.Errors;
    if (Global.Actions[Action_Encode]) {
        SingleFile.RAWcooked = &RAWcooked;
    }
    SingleFile.FileName = Name->substr(Global.Path_Pos_Global);
    FormatPath(SingleFile.FileName);

    // Parse
    if (ParseFile_Input((input_base&)SingleFile, FileMap, &InputInfo, !Global.Actions[Action_CheckPaddingOptionIsSet]))
        return true;
    if (!SingleFile.IsDetected() || SingleFile.ParserCode >= Uncompressed_Max)
        return false;

    // FFmpeg binary for encoding and FFmpeg libraries for the check after encoding, before the analysis of the other files
    if (!Global.CheckFFmpeg(false))
    {
        Global.ProgressIndicator_Stop();
        cerr << Global.FFmpeg_Error << endl;
        return true;
    }

    // Management
    Flavor = SingleFile.Flavor_String();
    if (SingleFile.IsSequence)
        Input.DetectSequence(Global.HasAtLeastOneFile, Files_Pos, RemovedFiles, Global.Path_Pos_Global, FileName_Template, FileName_StartNumber, FileName_EndNumber, FileList, Global.Actions, &Global.Errors);

    if (SingleFile.Version() == rawcooked::version::v2)
    {
        Global.ProgressIndicator_Stop();
        if (Global.Actions[Action_VersionValueIsAuto])
        {
            cerr << "Info: big reversibility file supported is needed,\n"
                << "      decoding is not possible with RAWcooked older than version 21.09.\n"
                << endl;

            if (!Global.Actions[Action_Check])
            {
                cerr << "Info: RAWcooked reversibility file version is set to 2.\n"
                    << endl;

                Global.SetVersion("2");
            }
        }

        if (!Global.Actions[Action_Check] && !Global.Actions[Action_CheckOptionIsSet])
        {
            cerr << "Info: this is a preview release,\n"
                << "      --check is set in order to verify the reversibility.\n"
                << endl;

            Global.SetCheck(true);
        }
    }

    if (SingleFile.ParserCode == Parser_DPX)
    {
        bool ForceCheck = false;
        if (dpx::IsVFlip(SingleFile.Flavor))
        {
            Global.OutputOptions["vf"] = "vflip"; // TODO: better management of such FFmpeg option
            ForceCheck = true; // TODO: quicker check of FFmpeg support of DPX orientation
        }
        switch ((dpx::flavor)SingleFile.Flavor)
        {
        case dpx::flavor::Raw_Y_10_FilledA_BE: // TODO: remove when we are confident enough, old FFmpeg are not fully compatible and future FFmpeg may change their behavior
        case dpx::flavor::Raw_Y_10_FilledB_BE:
        case dpx::flavor::Raw_Y_12_Packed_BE:
            ForceCheck = true; // TODO: quicker check of FFmpeg support of alternate EOL
            break;
        default:;
        }
        if (ForceCheck && !Global.Actions[Action_Check] && !Global.Actions[Action_CheckOptionIsSet])
        {
            cerr << "Info: this is a preview release,\n"
                << "      --check is set in order to verify the reversibility.\n"
                << endl;

            Global.SetCheck(true);
        }
    }

    if (SingleFile.ParserCode == Parser_AVI)
    {
        bool ForceCheck = true; // TODO: remove when we are confident enough
        if (ForceCheck && !Global.Actions[Action_Check] && !Global.Actions[Action_CheckOptionIsSet])
        {
            cerr << "Info: this is a preview release,\n"
                << "      --check is set in order to verify the reversibility.\n"
                << endl;

            Global.SetCheck(true);
        }
    }

    if (RemovedFiles.empty())
        RemovedFiles.push_back(Input.Files[Files_Pos]);
    else
    {
        // OverrideCheckPadding info
        bool OverrideCheckPadding = !Global.Actions[Action_CheckPadding] && SingleFile.RAWcooked && SingleFile.RAWcooked->HasInData();
        if (OverrideCheckPadding) // There are non-zero padding bits
        {
            Global.ProgressIndicator_Stop();
            cerr << "Info: non-zero padding bits found in first file,\n"
                << "      forcing the check of padding bits for all files.\n" << endl;
        }
        else if (SingleFile.ParserCode == Parser_DPX && dpx::MayHavePaddingBits((dpx::flavor)SingleFile.Flavor) && !Global.Actions[Action_CheckPaddingOptionIsSet] && !Global.Edits_Enabled) // If --no-checking-padding is not present, and padding bits are not relevant when editing
        {
            Global.ProgressIndicator_Stop();
            if (Global.Actions[Action_Check])
            {
                cerr << "Info: data can contain non-zero padding bits, padding bits will be\n"
                    "      checked only during reversibility check so after encoding,\n"
                    "      and will throw an error if non-zero padding bits are found\n"
                    "      at this moment, and in that case you'll have to re-encode with\n"
                    "      --check-padding option.\n" << endl;
            }
            else
            {
                cerr << "Error: data may contain non-zero padding bits but padding would never\n"
                    "       be tested with the current options, there is a risk of non-\n"
                    "       reversibility, use --no-check-padding for explicitly indicate\n"
                    "       that you are fine with that else use --check-padding for checking\n"
                    "       the padding bits before encoding, or alternatively use --check for\n"
                    "       checking padding bits after encoding (you'll have to re-encode\n"
                    "       with --check-padding option if non-zero padding bits are found).\n" << endl;
                return true;
            }
        }

        if (Global.Edits_Enabled && SingleFile.AddEdits(Global.Edits))
            return true;

        Global.ProgressIndicator_Start(Input.Files.size() + RemovedFiles.size() - 1);
        SingleFile.InputInfo->FrameCount = RemovedFiles.size();
        if (ParseFile_AdditionalInputs(SingleFile, FileMap, RemovedFiles, OverrideCheckPadding)) {
            return true;
        }
    }

    // License
    if (!Problem && Global.Actions[Action_Encode])
        Problem = !Global.License.IsSupported(SingleFile.ParserCode, (uint8_t)SingleFile.Flavor);

    // Technical limitations
    if (SingleFile.ParserCode >= Parser_WAV && SingleFile.ParserCode < Uncompressed_Max && ((input_base_uncompressed_audio*)&SingleFile)->BitDepth() > 24)
    {
        const auto& c_a = Global.OutputOptions.find("c:a");
        if (c_a == Global.OutputOptions.end())
        {
            Global.ProgressIndicator_Stop();
            cerr << "Info: FLAC encoding is not supported with " << to_string(((input_base_uncompressed_audio*)&SingleFile)->BitDepth()) << "-bit audio input,\n"
                    "      using audio copy (no compression).\n"<< endl;
            Global.OutputOptions["c:a"] = "copy";
        }
        else if (c_a->second == "flac")
        {
            Global.ProgressIndicator_Stop();
            cerr << "Error: FLAC encoding is not supported with " << to_string(((input_base_uncompressed_audio*)&SingleFile)->BitDepth()) << "-bit audio input,\n"
                    "       use -c:a copy.\n" << endl;
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------------------------
int ParseFile_Uncompressed(parse_info& ParseInfo, size_t Files_Pos)
{
    // Init
    RAWcooked.ResetTrack();

    for (int i = 0; i < Uncompressed_Max; i++) {
        auto Parser = CreateParser(i, &Global.Errors);
        if (!Parser)
            continue;
        auto NOK = ParseInfo.ParseFile_Input_Uncompressed(*Parser, Input, Files_Pos);
        if (!NOK && Parser->IsDetected()) {
            switch (i) {
            case Parser_AVI:
                Global.SetAcceptFiles();
                ParseInfo.IsContainer = true;
                ParseInfo.StreamCountMinus1 = ((avi*)Parser)->GetStreamCount() - 1;
                ParseInfo.FrameCount = ((avi*)Parser)->GetFrameCount();
                ParseInfo.FrameRate = ((avi*)Parser)->GetFrameRate();
                // fallthrough
            case Parser_DPX:
            case Parser_TIFF:
            case Parser_EXR:
                ParseInfo.Slices = std::to_string(Parser->slice_x * Parser->slice_y);
            }
            ParseInfo.IsDetected = true;
            delete Parser;
            break;
        }
        delete Parser;
        if (NOK) {
            return 1;
        }
    }

    // Unknown (but test if it is hashsum first)
    if (!ParseInfo.IsDetected)
    {
        bool HashFileParsed;
        if (Global.Actions[Action_Conch])
        {
            hashsum HashSum;
            HashSum.HomePath = ParseInfo.Name->substr(Global.Path_Pos_Global);
            FormatPath(HashSum.HomePath);
            HashSum.List = Global.Hashes;
            if (ParseInfo.ParseFile_Input_Uncompressed(HashSum, Input, Files_Pos))
                return 1;
            HashFileParsed = HashSum.IsDetected();
        }
        else
            HashFileParsed = false;
        if (HashFileParsed)
        {
            auto FileName = ParseInfo.Name->substr(Global.Path_Pos_Global);
            FormatPath(FileName);
            Global.Hashes->Ignore(FileName);
        }
        else
        {
            unknown Unknown;
            if (ParseInfo.ParseFile_Input_Uncompressed(Unknown, Input, Files_Pos))
                return 1;
        }
    }

    // End
    if (Global.HasAtLeastOneFile && !Global.AcceptFiles)
    {
        Global.ProgressIndicator_Stop();
        cerr << "Input is a file so directory will not be handled as a whole.\nConfirm that this is what you want to do by adding \" --file\" to the command." << endl;
        return 1;
    }
    if (ParseInfo.IsDetected)
    {
        stream Stream;

        Stream.FileName = ParseInfo.RemovedFiles[0].FileName;
        if (!ParseInfo.FileName_StartNumber.empty() && !ParseInfo.FileName_Template.empty())
        {
            Stream.FileName_Template = ParseInfo.FileName_Template;
            Stream.FileName_StartNumber = ParseInfo.FileName_StartNumber;
            Stream.FileName_EndNumber = ParseInfo.FileName_EndNumber;
            Stream.FileList = ParseInfo.FileList;
        }
        Stream.Flavor = ParseInfo.Flavor;
        Stream.Problem = ParseInfo.Problem;

        Stream.StreamCountMinus1 = ParseInfo.StreamCountMinus1;
        Stream.IsContainer = ParseInfo.IsContainer;
        Stream.Slices = ParseInfo.Slices;
        if (ParseInfo.IsContainer)
            Stream.FrameCount = ParseInfo.FrameCount;
        else if (!ParseInfo.Slices.empty()) // Video, a frame per file
            Stream.FrameCount = ParseInfo.RemovedFiles.size();
        map<string, string>::iterator FrameRateFromOptions = Global.VideoInputOptions.find("framerate");
        if (FrameRateFromOptions != Global.VideoInputOptions.end())
        {
            Stream.FrameRate = FrameRateFromOptions->second;
            ParseInfo.InputInfo.FrameRate = stod(Stream.FrameRate);
        }
        else if (ParseInfo.InputInfo.FrameRate)
            Stream.FrameRate = to_string(ParseInfo.InputInfo.FrameRate);

        Output.Streams.push_back(Stream);

        // Pre-set frame_size so ffmpeg's enc_open() does not set it to nb_samples,
        // which the FLAC encoder would reject as an invalid block size
        // if the audio stream has fewer samples than FLAC's minimum block size (16).
        if (ParseInfo.InputInfo.SampleCount > 0 && ParseInfo.InputInfo.SampleCount < 16)
        {
            if (Global.OutputOptions.find("frame_size") == Global.OutputOptions.end())
                Global.OutputOptions["frame_size"] = "16";
        }

        // Duration
        double Duration;
        if (ParseInfo.InputInfo.SampleCount && ParseInfo.InputInfo.SampleRate)
            Duration = ParseInfo.InputInfo.SampleCount / ParseInfo.InputInfo.SampleRate;
        else if (ParseInfo.InputInfo.FrameCount && ParseInfo.InputInfo.FrameRate)
            Duration = ParseInfo.InputInfo.FrameCount / ParseInfo.InputInfo.FrameRate;
        else
            Duration = 0;
        Output.Streams.back().Duration = (!Duration && ParseInfo.FrameCount && ParseInfo.FrameRate) ? ParseInfo.FrameCount / ParseInfo.FrameRate : Duration; // Also from the container (not for the coherency check)

        if (Global.Actions[Action_Coherency])
        {
            if (Duration)
            {
                Global.Durations.push_back(Duration);
                auto FileName = ParseInfo.Name->substr(Global.Path_Pos_Global);
                FormatPath(FileName);
                Global.Durations_FileName.push_back(FileName);
            }
        }
    }

    return 0;
}

//---------------------------------------------------------------------------
int ParseFile_Compressed(parse_info& ParseInfo, const string* FileOpenName)
{
    // Init
    string OutputDirectoryName;
    if (Global.OutputFileName.empty() && ParseInfo.Name)
        OutputDirectoryName = *ParseInfo.Name + ".RAWcooked" + PathSeparator;
    else
    {
        OutputDirectoryName = Global.OutputFileName;
        if (!OutputDirectoryName.empty() && OutputDirectoryName.find_last_of("/\\") != OutputDirectoryName.size() - 1)
            OutputDirectoryName += PathSeparator;
    }

    // Matroska
    int ReturnValue = 0;
    bool NoOutputCheck = !Global.Actions[Action_Decode] && !Global.OutputFileName_IsProvided;
    bool HasCheckedReversibility = !NoOutputCheck;
    auto DoesNotHaveReversibility = false;
    size_t Decoding_Frames[decoders_stats::Max] = {};
    double Decoding_Seconds = 0;
    double Decoding_Content_Seconds = 0;
    if (!ParseInfo.IsDetected)
    {
        // Threads
        unsigned threads;
        auto OutputOptions_Threads = Global.OutputOptions.find("threads");
        if (OutputOptions_Threads != Global.OutputOptions.end())
            threads = stoul(OutputOptions_Threads->second);
        else
            threads = 0;
        if (!threads)
            threads = thread::hardware_concurrency();
        ThreadPool* Thread_Pool;
        if (threads > 1)
        {
            Thread_Pool = new ThreadPool(threads, [] { trace::ThreadName("Pool"); });
            Thread_Pool->init();
        }
        else
            Thread_Pool = nullptr;

        matroska* M = new matroska(OutputDirectoryName, &Global.Mode, Ask_Callback, Thread_Pool, &Global.Errors);
        M->Quiet = Global.Quiet;
        M->ProgressIndicator_Summary = !Global.ShowStats; // Speed in the stats
        M->ProgressIndicator_Shares = Global.ShowStats;
        M->NoOutputCheck = NoOutputCheck;
        M->Decoders_Check = [] { return Global.CheckFFmpeg(true); }; // FFmpeg libraries for the FFmpeg decoders, before the decoding
        if (FileOpenName)
            M->FileName = *FileOpenName;
        M->OpenStyle = Global.FileOpenMethod;
        for (size_t i = 0; i < decoders_stats::Max; i++)
            Decoding_Frames[i] = DecodersStats.Frames[i];
        auto Decoding_Start = chrono::steady_clock::now();
        auto Decoding_NOK = ParseFile_Input(*M, ParseInfo.FileMap, &ParseInfo.InputInfo);
        Decoding_Seconds = chrono::duration<double>(chrono::steady_clock::now() - Decoding_Start).count();
        Decoding_Content_Seconds = M->Timestamp_Get() / 1000.0;
        for (size_t i = 0; i < decoders_stats::Max; i++)
            Decoding_Frames[i] = DecodersStats.Frames[i] - Decoding_Frames[i];
        if (Decoding_NOK)
        {
            ReturnValue = 1;
        }
        else if (M->Decoders_AreMissing)
        {
            cerr << '\n' << Global.FFmpeg_Error << endl;
            ParseInfo.IsDetected = true;
            DoesNotHaveReversibility = true; // No more info
            ReturnValue = 1;
        }
        else if (M->IsDetected())
        {
            ParseInfo.IsDetected = true;

            if (!HasCheckedReversibility && M->Hashes_FromRAWcooked)
                HasCheckedReversibility = true;

            if (Global.Actions[Action_Info] || Global.Log)
            {
                string Info;
                if (!M->RAWcooked_LibraryNameVersion_Get().empty())
                {
                    Info += "\nInfo: Reversibility data created by ";
                    Info += M->RAWcooked_LibraryNameVersion_Get();
                    Info += '.';
                }
                else if (Global.Actions[Action_Decode])
                {
                    Info += "\nInfo: No reversibility data found.";
                }
                if (M->Hashes_FromRAWcooked)
                {
                    Info += "\nInfo: Uncompressed file hashes (used by reversibility check) present.";
                }
                if (M->Hashes_FromAttachments && M->Hashes_FromAttachments->HashFiles_Count())
                {
                    Info += "\nInfo: ";
                    Info += to_string(M->Hashes_FromAttachments->HashFiles_Count());
                    Info += " hash file";
                    if (M->Hashes_FromAttachments->HashFiles_Count() > 1)
                        Info += 's';
                    Info += " (used by conformance check) found.";
                }
                cout << endl;
                if (Global.Actions[Action_Info])
                    cout << Info;
                if (Global.Log)
                    *Global.Log += Info;
            }
            if (Global.Actions[Action_Decode] || Global.Actions[Action_Check])
            {
                if (M->RAWcooked_LibraryNameVersion_Get().empty())
                {
                    auto Info = "\nError: No reversibility data found.";
                    cerr << Info;
                    if (Global.Log)
                        *Global.Log += Info;
                    ReturnValue = 1;
                    DoesNotHaveReversibility = true;
                }
            }
        }
        delete M;
        delete Thread_Pool;
    }

    // End
    string Info;
    if (ParseInfo.IsDetected && !Global.Quiet && !DoesNotHaveReversibility)
    {
        if (Global.Actions[Action_Decode])
        {
            cout << "\nInfo: Files are in ";
            cout << OutputDirectoryName;
            cout << ".";
        }
        if (Global.Actions[Action_Check] && !Global.Errors.HasErrors())
        {
            Info += "\nInfo: ";
            Info += (HasCheckedReversibility ? "Reversibility" : "Decoding");
            Info += " was checked, no issue detected.";
        }
    }
    if (Global.Actions[Action_Check] && Global.Errors.HasErrors())
    {
        Info += "\nInfo: ";
        Info += (HasCheckedReversibility ? "Reversibility" : "Decoding");
        Info += " was checked, issues detected, see below.";
    }
    if (Global.ShowStats && ParseInfo.IsDetected && !DoesNotHaveReversibility)
    {
        size_t Frames = 0;
        for (auto Decoder_Frames : Decoding_Frames)
            Frames += Decoder_Frames;
        if (Frames)
        {
            Info += "\nInfo: Decoding: ";
            Info += Stats_Speed(Frames, Decoding_Content_Seconds, Decoding_Seconds);

            // Share of each decoder if several decoders are used in parallel, else the decoder
            auto Shares = DecodersStats.Shares(Decoding_Frames);
            if (!Shares.empty())
            {
                Info += " (";
                Info += Shares;
                Info += ')';
            }
            else
            {
                static const char* const Names[decoders_stats::Max] = { "internal ffv1", "FFmpeg ffv1", "FFmpeg ffv1_vulkan" };
                for (size_t i = 0; i < decoders_stats::Max; i++)
                    if (Decoding_Frames[i])
                    {
                        Info += ", ";
                        Info += Names[i];
                    }
            }
            Info += '.';
            if (Decoding_Frames[decoders_stats::Vulkan])
            {
                Info += Stats_Vulkan_IsGPU ? "\n      GPU: " : "\n      Vulkan CPU implementation: ";
                Info += Stats_Vulkan_Name;
                Info += '.';
            }
            if (Decoding_Frames[decoders_stats::Ours] || Decoding_Frames[decoders_stats::FFmpeg])
                Info += Stats_CPU();
            Info += '\n';
        }
    }
    if (!Info.empty() && Info.back() != '\n')
        Info += '\n';
    cout << Info;
    if (Global.Log)
        *Global.Log += Info;

    return ReturnValue;
}

//---------------------------------------------------------------------------
int ParseFile(size_t Files_Pos)
{
    // Init
    parse_info ParseInfo;
    ParseInfo.Name = &Input.Files[Files_Pos].FileName;

    // Open file
    if (input::OpenInput(ParseInfo.FileMap, *ParseInfo.Name, Input.Files[Files_Pos].FileSize, &Global.Errors, Global.Edits_Enabled))
        return 1;

    // Compressed content
    if (int Value = ParseFile_Compressed(ParseInfo, ParseInfo.Name))
        return Value;
    if (ParseInfo.IsDetected)
        return 0;

    // Uncompressed content
    if (int Value = ParseFile_Uncompressed(ParseInfo, Files_Pos))
        return Value;
    if (ParseInfo.IsDetected)
        return 0;

    // Attachments
    size_t AttachmentSizeFinal = (Global.AttachmentMaxSize != (size_t)-1) ? Global.AttachmentMaxSize : (1024 * 1024); // Default value arbitrary chosen
    if (ParseInfo.FileMap.Size() >= AttachmentSizeFinal)
    {
        cout << "Error: " << *ParseInfo.Name << " is not small, expected to be an attachment?\nPlease contact info@mediaarea.net if you want support of such file." << endl;
        return 1;
    }
    if (!ParseInfo.FileMap.Empty()) // Ignoring file with size of 0
    {
        attachment Attachment;
        Attachment.FileName_In = *ParseInfo.Name;
        Attachment.FileName_Out = ParseInfo.Name->substr(Global.Path_Pos_Global);
        FormatPath(Attachment.FileName_Out);
        Output.Attachments.push_back(Attachment);
    }
    return 0;
}

//---------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
    // Global configuration
    Global.Ask_Callback = Ask_Callback;

    // Manage command line
    if (int Value = Global.ManageCommandLine(argv, argc))
        return Value;
    if (Global.ShowStats)
        DecodersStats.Vulkan_Selected = Stats_VulkanSelected;

    // Analyze input
    if (int Value = Input.AnalyzeInputs(Global))
        return Value;
    const auto FileEntry_Less = [](const FileEntry& l, const FileEntry& r) {return input::FileName_Compare(l.FileName, r.FileName) < 0; };
    if (!is_sorted(Input.Files.begin(), Input.Files.end(), FileEntry_Less)) // Often already in order (e.g. NTFS), a sort would compare the names again
        sort(Input.Files.begin(), Input.Files.end(), FileEntry_Less);

    // Parse files
    RAWcooked.FileName = Global.rawcooked_reversibility_FileName;
    if (Global.Actions[Action_VersionValueIsAuto])
        RAWcooked.Version = rawcooked::version::mini;
    else if (Global.Actions[Action_Version2]) 
        RAWcooked.Version = rawcooked::version::v2;
    int Value = 0;
    for (size_t i = 0; i < Input.Files.size(); i++)
    {
        Value = ParseFile(i);
        if (Value)
            break;
    }
    RAWcooked.Close();

    // Coherency checks
    if (Global.Actions[Action_Coherency])
    {
        input::CheckDurations(Global.Durations, Global.Durations_FileName, &Global.Errors);
    }

    // Hashes
    if (Global.Actions[Action_Hash])
    {
        Global.Hashes->NoMoreHashFiles();
        Global.Hashes->Finish();
        Global.Hashes->CheckFromFiles = true;
        Global.Hashes->WouldBeError = true;
    }

    // Progress indicator
    Global.ProgressIndicator_Stop();

    // Info
    if (!Global.Quiet)
    {
        string PrefixContent;
        auto ComputePrefix = [&](const string& FileName)
        {
            if (PrefixContent.size() > FileName.size())
                PrefixContent.resize(FileName.size());
            for (size_t j = 0; j < PrefixContent.size(); j++)
                if (PrefixContent[j] != FileName[j])
                {
                    PrefixContent.resize(j);
                    break;
                }
        };
        for (size_t i = 0; i < Output.Streams.size(); i++)
        {
            auto IsTemplate = !Output.Streams[i].FileName_Template.empty();
            const string& FileName = IsTemplate ? Output.Streams[i].FileName_Template : Output.Streams[i].FileName;
            if (!i)
            {
                PrefixContent = FileName;
                continue;
            }
            ComputePrefix(FileName);
        }
        for (const auto& Attachment : Output.Attachments)
        {
            ComputePrefix(Attachment.FileName_In);
        }
        auto PrefixSize = PrefixContent.size();

        if (Global.Log)
        {
            *Global.Log += GetLibraryName();
            *Global.Log += ' ';
            *Global.Log += GetLibraryVersion();
            if (Global.Actions[Action_Encode] && !Global.FFmpeg_Version().empty())
            {
                *Global.Log += ", FFmpeg ";
                *Global.Log += Global.FFmpeg_Version();
            }
            *Global.Log += "\nParameters used:";
            for (int i = 1; i < argc; i++)
            {
                if (Global.LogFile_IgnorePos.find(i) != Global.LogFile_IgnorePos.end())
                    continue;
                if (!strcmp(argv[i], "-b") || !strcmp(argv[i], "--bin-name"))
                {
                    i++;
                    continue;
                }
                *Global.Log += ' ';
                *Global.Log += argv[i];
            }
            *Global.Log += '\n';

            string PackageName;
            if (PrefixSize)
            {
                if (PrefixContent.find_first_of("\\/", PrefixContent.size() - 1) != string::npos)
                    PrefixContent.pop_back();
                PackageName = PrefixContent;
                if (!PackageName.empty())
                {
                    auto Pos = PackageName.find_last_of("\\/");
                    if (Pos != string::npos)
                        PackageName.erase(0, Pos + 1);
                }
            }
            *Global.Log += '\n';
            if (!PackageName.empty())
            {
                *Global.Log += "Package name: ";
                *Global.Log += PackageName;
                *Global.Log += '\n';
            }
        }

        string Message;
        const char* FlavorNotSupported = "\n *** This input format flavor is not supported by the current license key. ***";
        for (size_t i = 0; i < Output.Streams.size(); i++)
        {
            if (i)
                Message += '\n';
            Message += "Track ";
            Message += to_string(i + 1);
            Message += ":\n ";
            auto IsTemplate = !Output.Streams[i].FileName_Template.empty();
            const string& FileName = IsTemplate ? Output.Streams[i].FileName_Template : Output.Streams[i].FileName;
            #if defined(_WIN32) || defined(_WINDOWS)
            auto FileName2 = FileName.substr(PrefixSize);
            size_t Pos = 0;
            for (;;)
            {
                Pos = FileName2.find('\\');
                if (Pos == string::npos)
                    break;
                FileName2[Pos] = '/';
                Pos++;
            }
            Message += FileName2;
            #else
            Message += FileName.substr(PrefixSize);
            #endif
            if (IsTemplate)
            {
                Message += "\n (";
                Message += Output.Streams[i].FileName_StartNumber;
                Message += " --> ";
                Message += Output.Streams[i].FileName_EndNumber;
                if (!Output.Streams[i].FileList.empty())
                    Message += ", with gaps";
                Message += ')';
            }
            Message += "\n ";
            Message += Output.Streams[i].Flavor;
            if (Output.Streams[i].Problem)
                Message += FlavorNotSupported;
        }
        if (!Output.Attachments.empty())
        {
            Message += "\nAttachments:";
            for (const auto& Attachment : Output.Attachments)
            {
                Message += "\n ";
                Message += Attachment.FileName_In.substr(PrefixSize);
            }
        }
        cerr << Message << endl;
        if (Global.Log)
        {
            for (;;)
            {
                auto Pos = Message.find(FlavorNotSupported);
                if (Pos == string::npos)
                    break;
                Message.erase(Pos, strlen(FlavorNotSupported));
            }
            *Global.Log += Message;
            *Global.Log += '\n';
        }
    }

    if (!Value && Global.Errors.HasWarnings())
    {
        cerr << Global.Errors.ErrorMessage() << endl;
        switch (Global.Mode)
        {
        case AlwaysNo: Value = 1; break;
        case AlwaysYes: break;
        default:
            if ((!Value && Global.Actions[Action_Encode] && !Output.Streams.empty())
                || (Global.Actions[Action_Check] && !Global.Errors.HasErrors() && !Global.OutputFileName.empty() && !Output.Streams.empty()))
            {
                cerr << "Do you want to continue despite warnings? [y/N] ";
                string Result;
                getline(cin, Result);
                if (!(!Result.empty() && (Result[0] == 'Y' || Result[0] == 'y')))
                    Value = 1;
            }
        }
    }


    // FFmpeg
    if (!Value && Global.Actions[Action_Encode])
    {
        auto Encoding_Start = chrono::steady_clock::now();
        Value = Output.Process(Global, RAWcooked.Version == rawcooked::version::v2);
        auto Encoding_Seconds = chrono::duration<double>(chrono::steady_clock::now() - Encoding_Start).count();
        if (!Value && Global.ShowStats && !Global.DisplayCommand && !Output.Streams.empty())
        {
            // Duration of the content, with the frame rate given to FFmpeg if the input files have none
            double FrameRate = 0;
            auto FrameRate_Option = Global.VideoInputOptions.find("framerate");
            if (FrameRate_Option != Global.VideoInputOptions.end())
            {
                FrameRate = atof(FrameRate_Option->second.c_str());
                auto Slash = FrameRate_Option->second.find('/');
                if (Slash != string::npos)
                {
                    auto Denominator = atof(FrameRate_Option->second.c_str() + Slash + 1);
                    FrameRate = Denominator ? FrameRate / Denominator : 0;
                }
            }
            size_t Frames = 0;
            double Content_Seconds = 0;
            for (const auto& Stream : Output.Streams)
            {
                Frames += Stream.FrameCount;
                auto Duration = Stream.Duration;
                if (!Duration && Stream.FrameCount && !Stream.IsContainer && FrameRate > 0)
                    Duration = Stream.FrameCount / FrameRate;
                Content_Seconds = max(Content_Seconds, Duration);
            }
            string Info = "\nInfo: Encoding: " + Stats_Speed(Frames, Content_Seconds, Encoding_Seconds);
            const auto c_v = Global.OutputOptions.find("c:v");
            Info += ", FFmpeg ";
            Info += c_v == Global.OutputOptions.end() ? string("ffv1") : c_v->second;
            Info += '.';
            if (Global.Actions[Action_VulkanEncoder])
            {
                // Name of the device if it is the first one (default device of FFmpeg), from FFmpeg libraries
                const auto Device = Global.OutputOptions.find("init_hw_device");
                bool IsGPU = false;
                string Name;
                if (Device != Global.OutputOptions.end() && Device->second.find("vulkan=vk:0") != string::npos)
                    Name = FFmpeg_Library_VulkanName(&IsGPU);
                if (!Name.empty())
                {
                    Info += IsGPU ? "\n      GPU: " : "\n      Vulkan CPU implementation: ";
                    Info += Name;
                    Info += '.';
                }
            }
            else
                Info += Stats_CPU();
            Info += '\n';
            cout << Info << endl; // Empty line before the check
            if (Global.Log)
                *Global.Log += Info;
        }
    }

    // Handling of big reversibility files
    if (!Value && Global.Actions[Action_Encode] && RAWcooked.Version == rawcooked::version::v2 && !Output.Streams.empty())
    {
        rawcooked RAWcooked_Big;
        RAWcooked_Big.Version = rawcooked::version::v2;
        filemap ReversibilityFile;
        if (Global.DisplayCommand)
        {
            RAWcooked.Close();
            RAWcooked.Rename(Global.rawcooked_reversibility_FileName + ".tmp");
            ReversibilityFile.Open_ReadMode(RAWcooked.FileName);
            RAWcooked_Big.FileName = Global.rawcooked_reversibility_FileName;
        }
        else
        {
            ReversibilityFile.Open_ReadMode(Global.rawcooked_reversibility_FileName);
            RAWcooked_Big.FileName = Global.OutputFileName;
        }
        RAWcooked_Big.ReversibilityFile = &ReversibilityFile;
        RAWcooked_Big.Parse();
        if (Global.DisplayCommand)
        {
            ReversibilityFile.Close();
            RAWcooked.Delete();
        }
    }

    // RAWcooked file
    if (!Global.DisplayCommand)
        RAWcooked.Delete();

    // Check result
    if (!Value && !Global.DisplayCommand && (!Global.Actions[Action_CheckOptionIsSet] || Global.Actions[Action_Check]) && !Global.Errors.HasErrors() && !Global.OutputFileName.empty() && !Output.Streams.empty())
    {
        parse_info ParseInfo;
        Value = ParseInfo.FileMap.Open_ReadMode(Global.OutputFileName);
        if (!Value)
        {
            // Configure for a 2nd pass
            auto FileOpenName = Global.OutputFileName;
            ParseInfo.Name = NULL;
            Global.OutputFileName = Global.Inputs[0];
            if (!Global.Actions[Action_Hash]) // If hashes are present in the file, output is checked by using hashes
                Global.OutputFileName_IsProvided = true;

            // Remove directory name (already in RAWcooked file data)
            size_t Path_Pos = Global.OutputFileName.find_last_of("/\\");
            if (Path_Pos == Global.OutputFileName.size() - 1 && Global.OutputFileName.size() > 1)
                Path_Pos = Global.OutputFileName.find_last_of("/\\", Global.OutputFileName.size() - 2); // Ignore the trailing path separator
            if (Path_Pos == (size_t)-1)
                Path_Pos = 0;
            Global.OutputFileName.resize(Path_Pos);

            // Parse (check mode)
            Global.Actions.set(Action_QuickCheckAfterEncode, !Global.Actions[Action_Check]);
            Global.Actions.set(Action_Decode, false); // Override config
            Value = ParseFile_Compressed(ParseInfo, &FileOpenName);
            if (!Value && !ParseInfo.IsDetected)
            {
                cout << '\n' << "Error: " << Global.OutputFileName << endl;
                cout << "       output file can not be checked! Is it fine?" << endl;
                Value = 1;
            }
        }
    }

    // Global errors
    if (Global.Errors.HasErrors())
    {
        cerr << Global.Errors.ErrorMessage() << endl;
        if (!Value)
            Value = 1;
    }

    // Log
    if (Global.Log)
    {
        file LogFile;
        if (LogFile.Open_WriteMode(Global.LogFileName, string(), true))
        {
            if (Global.Mode == AlwaysNo)
            {
                cerr << "Error: File '" << Global.LogFileName << "' already exists.\n";
                Value = 1;
            }
            else
            {
                bool WriteLog = false;
                if (Global.Mode != AlwaysYes)
                {
                    cerr << "File '" << Global.LogFileName << "' already exists. Overwrite? [y/N] ";
                    string Result;
                    getline(cin, Result);
                    if (!Result.empty() && (Result[0] == 'Y' || Result[0] == 'y'))
                        WriteLog = true;
                    else
                        Value = 1;
                }
                else
                    WriteLog = true;
                if (WriteLog)
                    LogFile.Open_WriteMode(Global.LogFileName, string(), false, true);
            }
        }
        LogFile.Write((const uint8_t*)Global.Log->c_str(), Global.Log->size());
    }

    return Value;
}
