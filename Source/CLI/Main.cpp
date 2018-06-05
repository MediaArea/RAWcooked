/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
*
*  Use of this source code is governed by a BSD-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#include "CLI/Global.h"
#include "CLI/Input.h"
#include "CLI/Output.h"
#include "Lib/FileIO.h"
#include "Lib/Matroska/Matroska_Common.h"
#include "Lib/DPX/DPX.h"
#include "Lib/RIFF/RIFF.h"
#include "Lib/FFV1/FFV1_Frame.h"
#include "Lib/RawFrame/RawFrame.h"
#include "Lib/RAWcooked/RAWcooked.h"
#include <map>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <iostream>
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
global Global;
input Input;
output Output;
rawcooked RAWcooked;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
int ParseFile(size_t Files_Pos)
{
    RAWcooked.ResetTrack();
    
    string Name = Input.Files[Files_Pos];
    
    filemap FileMap;
    if (int Value = FileMap.Open_ReadMode(Name))
        return Value;

    matroska M;
    M.FileName = Name;
    M.Buffer = FileMap.Buffer;
    M.Buffer_Size = FileMap.Buffer_Size;
    M.Parse();

    vector<string> RemovedFiles;
    string FileName_Template;
    string FileName_StartNumber;
    string Slices;
    string FrameRate;

    map<string, string>::iterator FrameRateFromOptions = Global.VideoInputOptions.find("framerate");
    if (FrameRateFromOptions != Global.VideoInputOptions.end())
        FrameRate = FrameRateFromOptions->second;

    riff RIFF;
    if (!M.IsDetected)
    {
        RAWcooked.FileNameDPX = Name.substr(Global.Path_Pos_Global);

        RIFF.RAWcooked = &RAWcooked;
        RIFF.Buffer = FileMap.Buffer;
        RIFF.Buffer_Size = FileMap.Buffer_Size;
        RIFF.Parse();
        if (RIFF.ErrorMessage())
        {
            cerr << "Untested " << RIFF.ErrorMessage() << ", please contact info@mediaarea.net if you want support of such file\n";
            return 1;
        }

        if (RIFF.IsDetected)
        {
            RemovedFiles.push_back(Name);
        }
    }

    dpx DPX;
    if (!M.IsDetected && !RIFF.IsDetected)
    {
        Input.DetectSequence(Files_Pos, RemovedFiles, Global.Path_Pos_Global, FileName_Template, FileName_StartNumber);

        size_t i = 0;
        for (;;)
        {
            RAWcooked.FileNameDPX = Name.substr(Global.Path_Pos_Global);

            DPX.RAWcooked = &RAWcooked;
            DPX.Buffer = FileMap.Buffer;
            DPX.Buffer_Size = FileMap.Buffer_Size;
            DPX.FrameRate = FrameRate.empty() ? &FrameRate : NULL;
            DPX.Parse();
            if (DPX.ErrorMessage())
            {
                cerr << "Untested " << DPX.ErrorMessage() << ", please contact info@mediaarea.net if you want support of such file\n";
                return 1;
            }
            if (!DPX.IsDetected)
                break;
            if (!i)
            {
                stringstream t;
                t << DPX.slice_x * DPX.slice_y;
                Slices = t.str();
            }
            i++;

            if (i >= RemovedFiles.size())
                break;
            Name = RemovedFiles[i];
            FileMap.Open_ReadMode(Name);
        }
    }

    M.Shutdown();

    if (M.ErrorMessage())
    {
        cerr << "Untested " << M.ErrorMessage() << ", please contact info@mediaarea.net if you want support of such file\n";
        return 1;
    }

    // Processing DPX to MKV/FFV1
    if (!M.IsDetected)
    {
        if (!M.IsDetected && !RIFF.IsDetected && !DPX.IsDetected)
        {
            size_t AttachementSizeFinal = (Global.AttachementMaxSize != (size_t)-1) ? Global.AttachementMaxSize : (1024 * 1024); // Default value arbitrary choosen
            if (FileMap.Buffer_Size >= AttachementSizeFinal)
            {
                cout << Name << " is not small, expected to be an attachment? Please contact info@mediaarea.net if you want support of such file.\n";
                exit(1);
            }

            if (FileMap.Buffer_Size) // Ignoring file with size of 0
            {
                attachment Attachment;
                Attachment.FileName_In = Name;
                Attachment.FileName_Out = Name.substr(Global.Path_Pos_Global);
                Output.Attachments.push_back(Attachment);
            }
        }
        else
        {
            stream Stream;

            Stream.FileName = RemovedFiles[0];
            if (!FileName_StartNumber.empty() && !FileName_Template.empty())
            {
                Stream.FileName_StartNumber = FileName_StartNumber;
                Stream.FileName_Template = FileName_Template;
            }

            Stream.Slices = Slices;
            if (!Slices.empty() && FrameRateFromOptions == Global.VideoInputOptions.end())
                Stream.FrameRate = FrameRate;

            Output.Streams.push_back(Stream);
        }

    }
    else
        cout << "Files are in " << Name << ".RAWcooked" << endl;

    FileMap.Close();

    return 0;
}

//---------------------------------------------------------------------------
int main(int argc, const char* argv[])
{
    // Manage command line
    if (int Value = Global.ManageCommandLine(argv, argc))
        return Value;

    // Analyze input
    if (int Value = Input.AnalyzeInputs(Global))
        return Value;

    // Parse files
    RAWcooked.FileName = Global.rawcooked_reversibility_data_FileName;
    for (int i = 0; i < Input.Files.size(); i++)
        if (int Value = ParseFile(i))
            return Value;
    RAWcooked.Close();

    // FFmpeg
    if (int Value = Output.Process(Global))
        return Value;

    return 0;
}
