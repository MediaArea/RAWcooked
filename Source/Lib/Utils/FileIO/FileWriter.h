/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FileWriterH
#define FileWriterH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Utils/RawFrame/RawFrame.h"
#include "Lib/Utils/FileIO/FileIO.h"
#include <bitset>
class matroska;
using namespace std;
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
class frame_writer : public raw_frame_process
{
public:
    // Constructor / Destructor
    frame_writer(const string& BaseDirectory_Source, user_mode* UserMode_Soure, ask_callback Ask_Callback_Source, matroska* M_Source, errors* Errors_Source = nullptr) :
        BaseDirectory(BaseDirectory_Source),
        UserMode(UserMode_Soure),
        Ask_Callback(Ask_Callback_Source),
        M(M_Source),
        Errors(Errors_Source),
        MD5(nullptr)
    {
    }
    frame_writer(const frame_writer& Source) :
        Mode(Source.Mode),
        BaseDirectory(Source.BaseDirectory),
        UserMode(Source.UserMode),
        Ask_Callback(Source.Ask_Callback),
        M(Source.M),
        Errors(Source.Errors),
        Offset(Source.Offset),
        SizeOnDisk(Source.SizeOnDisk),
        MD5(Source.MD5)
    {
    }
    frame_writer(const frame_writer* Source) :
        frame_writer(*Source)
    {
    }

    virtual ~frame_writer();

    // Config
    enum mode
    {
        IsNotBegin,
        IsNotEnd,
        NoWrite,
        NoOutputCheck,
        NoHashCheck,
        mode_Max,
    };
    bitset<mode_Max>            Mode;
    string                      OutputFileName;

private:
    // Actions
    void                        FrameCall(raw_frame* RawFrame);

    bool                        WriteFile(raw_frame* RawFrame);
    bool                        CheckFile(raw_frame* RawFrame);
    bool                        CheckMD5(raw_frame* RawFrame);
    file                        File_Write;
    filemap                     File_Read;
    string                      BaseDirectory;
    user_mode*                  UserMode;
    ask_callback                Ask_Callback;
    matroska*                   M;
    errors*                     Errors;
    size_t                      Offset;
    size_t                      SizeOnDisk;
    void*                       MD5;
};

#endif