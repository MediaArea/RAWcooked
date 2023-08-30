/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/Output.h"
#include "Lib/Compressed/RAWcooked/IntermediateWrite.h"
#include <cmath>
#include <iostream>
#include <sstream>
#if defined(_WIN32) || defined(_WINDOWS)
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h>
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
int output::Process(global& Global, bool IgnoreReversibilityFile)
{
    if (!Streams.empty())
        return FFmpeg_Command(Global.Inputs[0].c_str(), Global, IgnoreReversibilityFile);
    else if (!Attachments.empty())
    {
        cerr << "Error: no A/V content detected.\nPlease contact info@mediaarea.net if you want support of such content." << endl;
        return 1;
    } 

    return 0;
}

//---------------------------------------------------------------------------
int output::FFmpeg_Command(const char* FileName, global& Global, bool IgnoreReversibilityFile)
{
    // Defaults
    if (int Value = Global.SetDefaults())
        return Value;
    if (Global.OutputOptions.find("slices") == Global.OutputOptions.end())
    {
        // Slice count depends on frame size
        string Slices;
        for (size_t i = 0; i < Streams.size(); i++)
        {
            if (Slices.empty() && !Streams[i].Slices.empty())
                Slices = Streams[i].Slices;
            if (!Streams[i].Slices.empty() && Streams[i].Slices != Slices)
            {
                cerr << "Error: untested multiple slices counts.\nPlease contact info@mediaarea.net if you want support of such file." << endl;
                return 1;
            }
        }
        if (!Slices.empty())
            Global.OutputOptions["slices"] = Slices;
    }
    if ((Streams.size() != 1 || !Streams[0].IsContainer) && Global.VideoInputOptions.find("framerate") == Global.VideoInputOptions.end())
    {
        // Looking if any video stream has a frame rate
        string FrameRate;
        for (size_t i = 0; i < Streams.size(); i++)
        {
            if (FrameRate.empty() && !Streams[i].FrameRate.empty())
                FrameRate = Streams[i].FrameRate;
            if (!Streams[i].Slices.empty() && !Streams[i].FrameRate.empty() && Streams[i].FrameRate != FrameRate) // Note: Slices part is used here for detecting video streams. TODO: more explicit track type flagging
            {
                cerr << "Error: untested multiple frame rates.\nPlease contact info@mediaarea.net if you want support of such file" << endl;
                return 1;
            }
        }
        if (!FrameRate.empty())
            Global.VideoInputOptions["framerate"] = FrameRate;
        else
            Global.VideoInputOptions["framerate"] = "24"; // Forcing framerate to 24 in case nothing is available in the input files and command line. TODO: find some autodetect of frame rate based on audio duration
    }
    auto FrameRate = Global.VideoInputOptions.find("framerate");
    if (FrameRate != Global.VideoInputOptions.end())
        Global.VideoInputOptions["r"] = FrameRate->second;

    string Command;
    if (Global.BinName.empty())
        Command += "ffmpeg";
    else
        Command += Global.BinName;
    Command += " -xerror";

    // Disable stdin for ffmpeg
    if (Global.OutputOptions.find("n") != Global.OutputOptions.end() && Global.OutputOptions.find("y") != Global.OutputOptions.end())
        Command += " -nostdin";

    // Info
    bool Problem = false;

    if (Streams.size() > 2 && !Global.License.IsSupported(feature::MultipleTracks))
    {
        if (!Global.Quiet)
            cerr << "*** More than 2 tracks is not supported by the current license key. ***" << endl;
        Problem = true;
    }

    // Input
    vector<intermediate_write*> FilesToRemove;
    for (size_t i = 0; i < Streams.size(); i++)
    {
        if (Streams[i].Problem)
            Problem = true;

        if (Streams[i].FileList.empty()) //Check if it is a template
        {
            // Input options
            if (!Streams[i].Slices.empty())
            {
                for (map<string, string>::iterator Option = Global.VideoInputOptions.begin(); Option != Global.VideoInputOptions.end(); Option++)
                    Command += " -" + Option->first + ' ' + Option->second;
            }

            // Force input format
            if (!Streams[i].Flavor.compare(0, 4, "DPX/"))
                Command += " -f image2 -c:v dpx";
            if (!Streams[i].Flavor.compare(0, 5, "TIFF/"))
                Command += " -f image2 -c:v tiff";
            if (!Streams[i].Flavor.compare(0, 4, "EXR/"))
            {
                Command += " -f image2 -c:v exr -consider_float16_as_uint16 1";
                Global.OutputOptions["metadata:s:v"] = "WARNING=\"Pixel content is IEEE 754 floating-point format\"";
            }

            // FileName_StartNumber (if needed)
            if (!Streams[i].FileName_StartNumber.empty())
            {
                Command += " -start_number ";
                Command += Streams[i].FileName_StartNumber;
            }

            // FileName_Template (is the file name if no sequence detected)
            Command += " -i \"";
            Command += Streams[i].FileName_Template.empty() ? Streams[i].FileName : Streams[i].FileName_Template;
            Command += "\"";
        }
        else // It is a list of files
        {
            // Input options
            if (!Streams[i].Slices.empty())
            {
                for (map<string, string>::iterator Option = Global.VideoInputOptions.begin(); Option != Global.VideoInputOptions.end(); Option++)
                {
                    if (Option->first == "framerate")
                    {
                        Command += " -r " + Option->second;

                        char* FrameRate_End;
                        auto FrameRate_Num = strtod(Option->second.c_str(), &FrameRate_End);
                        decltype(FrameRate_Num) FrameRate_Den;
                        if (FrameRate_End != Option->second.c_str() && *FrameRate_End == '/')
                            FrameRate_Den = strtod(FrameRate_End + 1, &FrameRate_End);
                        else
                            FrameRate_Den = 1;
                        if (!FrameRate_Num || !FrameRate_Den || FrameRate_Num / FrameRate_Den <= 0.5 || FrameRate_Num / FrameRate_Den > 1000 || FrameRate_End != Option->second.c_str() + Option->second.size())
                        {
                            cerr << "Error: issue with frame rate " << Option->second << ".\nPlease contact info@mediaarea.net if you want support of such content." << endl;
                            return 1;
                        }
                        if (FrameRate_Num / FrameRate_Den != 25)
                        {
                            int FrameTimeStamp_Stored = 0;
                            decltype(FrameRate_Num) FrameTimeStamp_Num = 0;

                            auto& FileList = Streams[i].FileList;
                            decltype(Streams[i].FileList) FileList_Temp;
                            FileList_Temp.reserve(FileList.size());
                            char* CurrentPath = nullptr;
                            for (size_t i = 0; i < FileList.size();)
                            {
                                auto j = FileList.find('\n', i);
                                if (j == string::npos)
                                    j = FileList.size();
                                FileList_Temp += "file '";
#if defined(_WIN32) || defined(_WINDOWS)
                                if (i + 2 < FileList.size() && FileList[i + 1] != ':' && FileList[i + 2] != PathSeparator && !CurrentPath)
#else
                                if (FileList[i] != PathSeparator && !CurrentPath)
#endif
                                {
                                    CurrentPath = new char[FILENAME_MAX];
                                    if (!getcwd(CurrentPath, FILENAME_MAX))
                                    {
                                        delete CurrentPath;
                                        CurrentPath = nullptr;
                                    }
                                }
                                if (CurrentPath)
                                {
                                    FileList_Temp.append(CurrentPath);
                                    FileList_Temp.append(1, PathSeparator);
                                }
                                FileList_Temp.append(FileList, i, j - i);
                                i = j + 1;
                                FileList_Temp += "'\nduration ";

                                FrameTimeStamp_Num += FrameRate_Den;
                                auto Duration_Temp = int(round(FrameTimeStamp_Num / FrameRate_Num * 1000000)) - FrameTimeStamp_Stored;
                                FileList_Temp += '0' + Duration_Temp / 1000000;
                                FileList_Temp += '.';
                                FileList_Temp += '0' + (Duration_Temp / 100000) % 10;
                                FileList_Temp += '0' + (Duration_Temp / 10000) % 10;
                                FileList_Temp += '0' + (Duration_Temp / 1000) % 10;
                                FileList_Temp += '0' + (Duration_Temp / 100) % 10;
                                FileList_Temp += '0' + (Duration_Temp / 10) % 10;
                                FileList_Temp += '0' + Duration_Temp % 10;
                                FileList_Temp += '\n';

                                FrameTimeStamp_Stored += Duration_Temp;
                                if (FrameTimeStamp_Stored >= 1000000)
                                {
                                    FrameTimeStamp_Stored -= 1000000;
                                    FrameTimeStamp_Num -= FrameRate_Num;
                                }
                            }
                            delete[] CurrentPath;
                            FileList = FileList_Temp;
                        }
                    }
                    else
                        Command += " -" + Option->first + ' ' + Option->second;
                }
            }

            // Force input format
            Command += " -f concat -safe 0";
            if (!Streams[i].Flavor.compare(0, 4, "DPX/"))
                Command += " -c:v dpx";
            if (!Streams[i].Flavor.compare(0, 5, "TIFF/"))
                Command += " -c:v tiff";
            if (!Streams[i].Flavor.compare(0, 4, "EXR/"))
                Command += " -c:v exr -consider_float16_as_uint16 1";

            // Write the list of files
            auto FileList_File = new intermediate_write;
            FileList_File->FileName = Global.rawcooked_reversibility_FileName;
            FileList_File->FileName += '.';
            FileList_File->FileName += to_string(i);
            FileList_File->FileName += ".FileList.txt";
            FileList_File->Errors = &Global.Errors;
            FileList_File->Mode = &Global.Mode;
            FileList_File->Ask_Callback = Global.Ask_Callback;
            FileList_File->WriteToDisk((const uint8_t*)Streams[i].FileList.c_str(), Streams[i].FileList.size());
            FileList_File->Close();
            FilesToRemove.push_back(FileList_File);

            Command += " -i \"";
            Command += FileList_File->FileName;
            Command += '\"';
        }

        if (Global.Errors.HasErrors())
            return 1;
    }

    // Map when there are several streams
    size_t MapPos = 0;
    if (Streams.size()>1)
    {
        for (size_t i = 0; i < Streams.size(); i++)
        {
            stringstream t;
            t << MapPos++;
            Command += " -map ";
            Command += t.str();
        }
    }
    else
        MapPos += Streams.front().StreamCountMinus1 + 1;

    // Output
    for (map<string, string>::iterator Option = Global.OutputOptions.begin(); Option != Global.OutputOptions.end(); Option++)
    {
        Command += " -" + Option->first;
        if (!Option->second.empty())
            Command += ' ' + Option->second;
    }
    for (size_t i = 0; i < Attachments.size(); i++)
    {
        // Info
        if (!Global.Quiet)
        {
            if (!i)
                cerr << "Attachments:" << endl;
            cerr << "  " << Attachments[i].FileName_Out.substr(Attachments[i].FileName_Out.find_first_of("/\\")+1) << endl;
        }

        if (!Global.Actions[Action_Version2])
        {
            stringstream t;
            t << MapPos++;
            Command += " -attach \"" + Attachments[i].FileName_In + "\" -metadata:s:" + t.str() + " mimetype=application/octet-stream -metadata:s:" + t.str() + " \"filename=" + Attachments[i].FileName_Out + "\"";
        }
    }
    stringstream t;
    t << MapPos++;
    if (!IgnoreReversibilityFile)
        Command += " -attach \"" + Global.rawcooked_reversibility_FileName + "\" -metadata:s:" + t.str() + " mimetype=application/octet-stream -metadata:s:" + t.str() + " \"filename=RAWcooked reversibility data\" ";
    if (Global.OutputFileName.empty())
    {
        Global.OutputFileName = FileName;
        if (Global.OutputFileName.back() == '/'
        #if defined(_WIN32) || defined(_WINDOWS)
            || Global.OutputFileName.back() == '\\'
        #endif // defined(_WIN32) || defined(_WINDOWS)
            )
            Global.OutputFileName.pop_back();
        Global.OutputFileName += ".mkv";
    }
    Command += " -f matroska \"";
    Command += Global.OutputFileName;
    Command += '\"';
    for (const auto& Option : Global.MoreOutputOptions)
    {
        Command += ' ';
        Command += Option;
    }

    if (Global.Actions[Action_FrameMd5])
    {
        if (Global.FrameMd5FileName.empty())
        {
            Global.FrameMd5FileName = FileName;
            if (Global.FrameMd5FileName.back() == '/'
            #if defined(_WIN32) || defined(_WINDOWS)
                || Global.FrameMd5FileName.back() == '\\'
            #endif // defined(_WIN32) || defined(_WINDOWS)
                )
                Global.FrameMd5FileName.pop_back();
            Global.FrameMd5FileName += ".framemd5";
        }
        if (Global.Actions[Action_FrameMd5An])
        {
            Command += " -an";
        }
        Command += " -f framemd5 \"";
        Command += Global.FrameMd5FileName;
        Command += '\"';
    }

    // Info
    if (Problem)
    {
        cerr << "\nOne or more requested features are not supported with the current license key.\n";
        cerr << "Please contact info@mediaarea.net for a quote or a temporary key." << endl;
        if (!Global.IgnoreLicenseKey)
            return 1;
        cerr << "Ignoring the license for the moment." << endl;
    }
    if (!Global.Quiet || Problem)
    {
        cerr << endl;
    }

    if (Global.DisplayCommand)
    {
        cout << Command;
        if (Global.Actions[Action_Version2])
            cout << " && cat " << Global.rawcooked_reversibility_FileName << " >> " << Global.OutputFileName << " && rm " << Global.rawcooked_reversibility_FileName;
    }
    else
    {
        int Value = system(Command.c_str());
        #if !(defined(_WIN32) || defined(_WINDOWS))
        if (Value > 0xFF && !(Value & 0xFF))
                Value++; // On Unix-like systems, exit status code is sometimes casted to 8-bit long, and system() returns a value multiple of 0x100 when e.g. the command does not exist. We increment the value by 1 in order to have cast to 8-bit not 0 (which can be considered as "OK" by some commands e.g. appending " && echo OK")
        #endif

        // Delete temporary files
        for (auto FileToRemove: FilesToRemove)
        {
            if (FileToRemove->Delete())
            {
                if (!Value) // Prioritizing FFmpeg error code
                    Value = 1;
            }
            delete FileToRemove;
        }
        FilesToRemove.clear();
    
        return Value;
    }

    return 0;
}

