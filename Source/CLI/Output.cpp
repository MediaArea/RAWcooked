/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "CLI/Output.h"
#include <iostream>
#include <sstream>
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
int output::Process(global& Global)
{
    if (!Streams.empty())
        return FFmpeg_Command(Global.Inputs[0].c_str(), Global);
    else if (!Attachments.empty())
    {
        cerr << "No A/V content detected, please contact info@mediaarea.net if you want support of such content\n";
        return 1;
    }

    return 0;
}

//---------------------------------------------------------------------------
int output::FFmpeg_Command(const char* FileName, global& Global)
{
    // Defaults
    Global.SetDefaults();
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
                cerr << "Untested multiple slices counts, please contact info@mediaarea.net if you want support of such file\n";
                return 1;
            }
        }
        if (!Slices.empty())
            Global.OutputOptions["slices"] = Slices;
    }

    string Command;
    if (Global.BinName.empty())
        Command += "ffmpeg";
    else
        Command += Global.BinName;

    // Input
    for (size_t i = 0; i < Streams.size(); i++)
    {
        if (!Streams[i].Slices.empty())
        {
            for (map<string, string>::iterator Option = Global.VideoInputOptions.begin(); Option != Global.VideoInputOptions.end(); Option++)
                Command += " -" + Option->first + ' ' + Option->second;
        }

        // FileName_StartNumber (if needed)
        if (!Streams[i].FileName_StartNumber.empty())
        {
            if (!Streams[i].FrameRate.empty())
            {
                Command += " -framerate ";
                Command += Streams[i].FrameRate;
            }
            Command += " -start_number ";
            Command += Streams[i].FileName_StartNumber;
        }

        // FileName_Template (is the file name if no sequence detected)
        Command += " -i \"";
        Command += Streams[i].FileName_Template.empty() ? Streams[i].FileName : Streams[i].FileName_Template;
        Command += "\"";
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
        MapPos++;

    // Output
    for (map<string, string>::iterator Option = Global.OutputOptions.begin(); Option != Global.OutputOptions.end(); Option++)
        Command += " -" + Option->first + ' ' + Option->second;
    for (size_t i = 0; i < Attachments.size(); i++)
    {
        stringstream t;
        t << MapPos++;
        Command += " -attach \"" + Attachments[i].FileName_In + "\" -metadata:s:" + t.str() + " mimetype=application/octet-stream -metadata:s:" + t.str() + " \"filename=" + Attachments[i].FileName_Out + "\"";
    }
    stringstream t;
    t << MapPos++;
    Command += " -attach \"" + Global.rawcooked_reversibility_data_FileName + "\" -metadata:s:" + t.str() + " mimetype=application/octet-stream -metadata:s:" + t.str() + " \"filename=RAWcooked reversibility data\" \"";
    if (Global.OutputFileName.empty())
    {
        Command += FileName;
        if (Command[Command.size() - 1] == '/' || Command[Command.size() - 1] == '\\')
            Command.pop_back();
        Command += ".mkv";
    }
    else
    {
        Command += Global.OutputFileName;
    }
    Command += '\"';

    if (Global.DisplayCommand)
        cout << Command << endl;
    else
    {
        if (int Value = system(Command.c_str()))
        {
            cout << Value << endl;
            #if !(defined(_WIN32) || defined(_WINDOWS))
                if (Value > 0xFF && !(Value & 0xFF))
                   Value++; // On Unix-like systems, exit status code is sometimes casted to 8-bit long, and system() returns a value multiple of 0x100 when e.g. the command does not exist. We increment the value by 1 in order to have cast to 8-bit not 0 (which can be considered as "OK" by some commands e.g. appending " && echo OK")
            #endif
            return Value;
        }

        // Delete the temporary file
        int Result = remove(Global.rawcooked_reversibility_data_FileName.c_str());
        if (Result)
            cerr << "Error: can not remove temporary file " << Global.rawcooked_reversibility_data_FileName << endl;
    }

    return 0;
}

