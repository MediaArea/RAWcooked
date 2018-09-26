/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/Global.h"
#include "CLI/Help.h"
#include <iostream>
#include <cstring>
#include <iomanip>
#include <thread>
#if defined(_WIN32) || defined(_WINDOWS)
    #include <direct.h>
    #define getcwd _getcwd
#else
    #include <unistd.h>
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Glue
void global_ProgressIndicator_Show(global* G)
{
    G->ProgressIndicator_Show();
}

//---------------------------------------------------------------------------
int global::SetOutputFileName(const char* FileName)
{
    OutputFileName = FileName;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetBinName(const char* FileName)
{
    BinName = FileName;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetDisplayCommand()
{
    DisplayCommand = true;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetAcceptFiles()
{
    AcceptFiles = true;
    return 0;
}

//---------------------------------------------------------------------------
int global::SetFullCheck(const char* Value)
{
    if (strcmp(Value, "0") == 0 || strcmp(Value, "partial") == 0)
    {
        FullCheck = false;
        return 0;
    }
    if (strcmp(Value, "1") == 0 || strcmp(Value, "full") == 0)
    {
        FullCheck = true;
        return 0;
    }
    cerr << "Invalid --check value." << endl;
    return 1;
}

//---------------------------------------------------------------------------
int Error_NotTested(const char* Option1, const char* Option2 = NULL)
{
    cerr << "Error: option " << Option1;
    if (Option2)
        cerr << ' ' << Option2;
    cerr << " not yet tested.\nPlease contact info@mediaarea.net if you want support of such option." << endl;
    return 1;
}

//---------------------------------------------------------------------------
int global::SetOption(const char* argv[], int& i, int argc)
{
    if (strcmp(argv[i], "-c:a") == 0)
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (strcmp(argv[i], "copy") == 0)
        {
            OutputOptions["c:a"] = argv[i];
            return 0;
        }
        if (strcmp(argv[i], "flac") == 0)
        {
            OutputOptions["c:a"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-c:v"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "ffv1"))
        {
            OutputOptions["c:v"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-coder"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "0")
         || !strcmp(argv[i], "1")
         || !strcmp(argv[i], "2"))
        {
            OutputOptions["coder"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-context"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "0")
         || !strcmp(argv[i], "1"))
        {
            OutputOptions["context"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-f"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "matroska"))
        {
            OutputOptions["f"] = argv[i];
            return 0;
        }
        return 0;
    }
    if (!strcmp(argv[i], "-framerate"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (atof(argv[i]))
        {
            VideoInputOptions["framerate"] = argv[i];
            return 0;
        }
        return 0;
    }
    if (!strcmp(argv[i], "-g"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (atoi(argv[i]))
        {
            OutputOptions["g"] = argv[i];
            return 0;
        }
        cerr << "Invalid \"" << argv[i - 1] << " " << argv[i] << "\" value, it must be a number\n";
        return 1;
    }
    if (!strcmp(argv[i], "-level"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "0")
         || !strcmp(argv[i], "1")
         || !strcmp(argv[i], "3"))
        {
            OutputOptions["level"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-loglevel"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        if (!strcmp(argv[i], "error")
         || !strcmp(argv[i], "warning"))
        {
            OutputOptions["loglevel"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-slicecrc"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        int SliceCount = atoi(argv[i]);
        if (!strcmp(argv[i], "0")
         || !strcmp(argv[i], "1"))
        {
            OutputOptions["slicecrc"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-slices"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        int SliceCount = atoi(argv[i]);
        if (SliceCount) //TODO: not all slice counts are accepted by FFmpeg, we should filter
        {
            OutputOptions["slices"] = argv[i];
            return 0;
        }
        return Error_NotTested(argv[i - 1], argv[i]);
    }
    if (!strcmp(argv[i], "-threads"))
    {
        if (++i >= argc)
            return Error_NotTested(argv[i - 1]);
        OutputOptions["threads"] = argv[i];
        return 0;
    }

    return Error_NotTested(argv[i]);
}

//---------------------------------------------------------------------------
int global::ManageCommandLine(const char* argv[], int argc)
{
    if (argc < 2)
        return Usage(argv[0]);

    AttachmentMaxSize = (size_t)-1;
    DisplayCommand = false;
    AcceptFiles = false;
    FullCheck = false;
    Quiet = false;
    ProgressIndicator_Thread = NULL;

    for (int i = 1; i < argc; i++)
    {
        if ((argv[i][0] == '.' && argv[i][1] == '\0')
         || (argv[i][0] == '.' && (argv[i][1] == '/' || argv[i][1] == '\\') && argv[i][2] == '\0'))
        {
            //translate to "../xxx" in order to get the top level directory name
            char buff[FILENAME_MAX];
            if (getcwd(buff, FILENAME_MAX))
            {
                string Arg = buff;
                size_t Path_Pos = Arg.find_last_of("/\\");
                Arg = ".." + Arg.substr(Path_Pos);
                Inputs.push_back(Arg);
            }
            else
            {
                cerr << "Error: " << argv[i] << " can not be transformed to a directory name." << endl;
                return 1;
            }
        }
        else if ((strcmp(argv[i], "--attachment-max-size") == 0 || strcmp(argv[i], "-s") == 0) && i + 1 < argc)
        {
            AttachmentMaxSize = atoi(argv[++i]);
        }
        else if ((strcmp(argv[i], "--bin-name") == 0 || strcmp(argv[i], "-b") == 0) && i + 1 < argc)
        {
            int Value = SetBinName(argv[++i]);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--check") == 0 && i + 1 < argc)
        {
            int Value = SetFullCheck(argv[++i]);
            if (Value)
                return Value;
        }
        else if ((strcmp(argv[i], "--display-command") == 0 || strcmp(argv[i], "-d") == 0))
        {
            int Value = SetDisplayCommand();
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--file") == 0)
        {
            int Value = SetAcceptFiles();
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            int Value = Help(argv[0]);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--version") == 0)
        {
            int Value = Version();
            if (Value)
                return Value;
        }
        else if ((strcmp(argv[i], "--output-name") == 0 || strcmp(argv[i], "-o") == 0) && i + 1 < argc)
        {
            int Value = SetOutputFileName(argv[++i]);
            if (Value)
                return Value;
        }
        else if (strcmp(argv[i], "--quiet") == 0)
        {
            Quiet = true;
        }
        else if ((strcmp(argv[i], "--rawcooked-file-name") == 0 || strcmp(argv[i], "-r") == 0) && i + 1 < argc)
            rawcooked_reversibility_data_FileName = argv[++i];
        else if (argv[i][0] == '-' && argv[i][1] != '\0')
        {
            int Value = SetOption(argv, i, argc);
            if (Value)
                return Value;
        }
        else
            Inputs.push_back(argv[i]);
    }

    return 0;
}

//---------------------------------------------------------------------------
int global::SetDefaults()
{
    // Container format
    if (OutputOptions.find("f") == OutputOptions.end())
        OutputOptions["f"] = "matroska"; // Container format is Matroska

    // Video format
    if (OutputOptions.find("c:v") == OutputOptions.end())
        OutputOptions["c:v"] = "ffv1"; // Video format is FFV1

    // Audio format
    if (OutputOptions.find("c:a") == OutputOptions.end())
        OutputOptions["c:a"] = "flac"; // Audio format is FLAC
                                 
    // FFV1 specific
    if (OutputOptions["c:v"] == "ffv1")
    {
        if (OutputOptions.find("coder") == OutputOptions.end())
            OutputOptions["coder"] = "1"; // Range Coder
        if (OutputOptions.find("context") == OutputOptions.end())
            OutputOptions["context"] = "1"; // Small context
        if (OutputOptions.find("g") == OutputOptions.end())
            OutputOptions["g"] = "1"; // Intra
        if (OutputOptions.find("level") == OutputOptions.end())
            OutputOptions["level"] = "3"; // FFV1 v3
        if (OutputOptions.find("slicecrc") == OutputOptions.end())
            OutputOptions["slicecrc"] = "1"; // Slice CRC on

        // Check incompatible options
        if (OutputOptions["level"] == "0" || OutputOptions["level"] == "1")
        {
            map<string, string>::iterator slices = OutputOptions.find("slices");
            if (slices == OutputOptions.end() || slices->second != "1")
            {
                cerr << "\" -level " << OutputOptions["level"] << "\" does not permit slices, is it intended ? if so, add \" -slices 1\" to the command." << endl;
                return 1;
            }
        }
    }

    return 0;
}

//---------------------------------------------------------------------------
void global::ProgressIndicator_Start(size_t Total)
{
    if (ProgressIndicator_Thread)
        return;
    ProgressIndicator_Current = 0;
    ProgressIndicator_Total = Total;
    ProgressIndicator_Thread = new thread(global_ProgressIndicator_Show, this);
}

//---------------------------------------------------------------------------
void global::ProgressIndicator_Stop()
{
    if (!ProgressIndicator_Thread)
        return;
    ProgressIndicator_Current = ProgressIndicator_Total;
    ProgressIndicator_IsEnd.notify_one();
    ProgressIndicator_Thread->join();
    delete ProgressIndicator_Thread;
    ProgressIndicator_Thread = NULL;
}

//---------------------------------------------------------------------------
// Progress indicator show
void global::ProgressIndicator_Show()
{
    // Configure progress indicator precision
    size_t ProgressIndicator_Value = (size_t)-1;
    size_t ProgressIndicator_Frequency = 100;
    streamsize Precision = 0;
    cerr.setf(ios::fixed, ios::floatfield);

    // Configure benches
    using namespace chrono;
    steady_clock::time_point Clock_Init = steady_clock::now();
    steady_clock::time_point Clock_Previous = Clock_Init;
    uint64_t FileCount_Previous = 0;

    // Show progress indicator at a specific frequency
    const chrono::seconds Frequency = chrono::seconds(1);
    size_t StallDetection = 0;
    mutex Mutex;
    unique_lock<mutex> Lock(Mutex);
    do
    {
        size_t ProgressIndicator_New = (size_t)(((float)ProgressIndicator_Current) * ProgressIndicator_Frequency / ProgressIndicator_Total);
        if (ProgressIndicator_New == ProgressIndicator_Value)
        {
            StallDetection++;
            if (StallDetection >= 4)
            {
                while (ProgressIndicator_New == ProgressIndicator_Value && ProgressIndicator_Frequency < 10000)
                {
                    ProgressIndicator_Frequency *= 10;
                    ProgressIndicator_Value *= 10;
                    Precision++;
                    ProgressIndicator_New = (size_t)(((float)ProgressIndicator_Current) * ProgressIndicator_Frequency / ProgressIndicator_Total);
                }
            }
        }
        else
            StallDetection = 0;
        if (ProgressIndicator_New != ProgressIndicator_Value)
        {
            float FileRate = 0;
            if (ProgressIndicator_Value != (size_t)-1)
            {
                steady_clock::time_point Clock_Current = steady_clock::now();
                steady_clock::duration Duration = Clock_Current - Clock_Previous;
                FileRate = (float)(ProgressIndicator_Current - FileCount_Previous) * 1000 / duration_cast<milliseconds>(Duration).count();
                Clock_Previous = Clock_Current;
                FileCount_Previous = ProgressIndicator_Current;
            }
            cerr << '\r';
            cerr << "Analyzing files (" << setprecision(Precision) << ((float)ProgressIndicator_New) * 100 / ProgressIndicator_Frequency << "%)";
            if (FileRate)
            {
                cerr << ", ";
                cerr << setprecision(0) << FileRate;
                cerr << " files/s";
            }
            cerr << "    "; // Clean up in case there is less content outputed than the previous time

            ProgressIndicator_Value = ProgressIndicator_New;
        }
    } while (ProgressIndicator_IsEnd.wait_for(Lock, Frequency) == cv_status::timeout, ProgressIndicator_Current != ProgressIndicator_Total);

    // Show summary
    cerr << '\r';
    cerr << "                              \r"; // Clean up in case there is less content outputed than the previous time
}
