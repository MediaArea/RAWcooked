/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Compressed/RAWcooked/IntermediateWrite.h"
#include "Lib/Common/Common.h"
using namespace std;

//---------------------------------------------------------------------------
// Errors

namespace intermediatewrite_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "cannot create directory",
    "cannot create file",
    "file already exists",
    "cannot write to the file",
    "cannot remove file",
    "file is becoming too big",
};

enum code : uint8_t
{
    CreateDirectory,
    FileCreate,
    FileAlreadyExists,
    FileWrite,
    FileRemove,
    FileBecomingTooBig,
    Max
};

namespace undecodable { static_assert(Max == sizeof(MessageText) / sizeof(const char*), IncoherencyMessage); }

} // unparsable

const char** ErrorTexts[] =
{
    undecodable::MessageText,
    nullptr,
    nullptr,
    nullptr,
};

static_assert(error::type_Max == sizeof(ErrorTexts) / sizeof(const char**), IncoherencyMessage);

} // intermediatewrite_issue

//---------------------------------------------------------------------------
intermediate_write::intermediate_write()
{
}

//---------------------------------------------------------------------------
intermediate_write::~intermediate_write()
{
    Close();
}

//---------------------------------------------------------------------------
bool intermediate_write::Close()
{
    File.SetEndOfFile();
    File.Close();

    return false;
}

//---------------------------------------------------------------------------
bool intermediate_write::Delete()
{
    if (!File_WasCreated)
        return true;

    // Delete the temporary file
    int Result = remove(FileName.c_str());
    if (Result)
    {
        if (Errors)
            Errors->Error(IO_IntermediateWriter, error::type::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileRemove, FileName);
        return true;
    }

    return false;
}

//---------------------------------------------------------------------------
bool intermediate_write::Rename(const string& NewFileName)
{
    if (!File_WasCreated)
        return true;

    // Delete the temporary file
    int Result = rename(FileName.c_str(), NewFileName.c_str());
    if (Result)
    {
        if (Errors)
            Errors->Error(IO_IntermediateWriter, error::type::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileRemove, FileName);
        return true;
    }

    FileName = NewFileName;
    return false;
}

//---------------------------------------------------------------------------
void intermediate_write::SetErrorFileBecomingTooBig()
{
    if (Errors)
    {
        Errors->Error(IO_IntermediateWriter, error::type::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileBecomingTooBig, FileName);
    }
}

//---------------------------------------------------------------------------
void intermediate_write::WriteToDisk(const uint8_t* Buffer, size_t Buffer_Size)
{
    if (!File_WasCreated)
    {
        bool RejectIfExists = (Mode && *Mode == AlwaysYes) ? false : true;
        if (file::return_value Result = File.Open_WriteMode(string(), FileName, RejectIfExists))
        {
            if (RejectIfExists && Result == file::Error_FileAlreadyExists && Mode && *Mode == Ask && Ask_Callback)
            {
                user_mode NewMode = Ask_Callback(Mode, FileName, string(), false, ProgressIndicator_IsPaused, ProgressIndicator_IsEnd);
                if (NewMode == AlwaysYes)
                    Result = File.Open_WriteMode(string(), FileName, false);
            }

            if (Result)
            {
                if (Errors)
                    switch (Result)
                    {
                        case file::Error_CreateDirectory:       Errors->Error(IO_IntermediateWriter, error::type::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::CreateDirectory, FileName); break;
                        case file::Error_FileCreate:            Errors->Error(IO_IntermediateWriter, error::type::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileCreate, FileName); break;
                        case file::Error_FileAlreadyExists:     Errors->Error(IO_IntermediateWriter, error::type::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileAlreadyExists, FileName); break;
                        default:                                Errors->Error(IO_IntermediateWriter, error::type::Undecodable, (error::generic::code)intermediatewrite_issue::undecodable::FileWrite, FileName); break;
                    }
                return;
            }
        }

        File_WasCreated = true;
    }

    if (File.Write(Buffer, Buffer_Size))
        return;
}
