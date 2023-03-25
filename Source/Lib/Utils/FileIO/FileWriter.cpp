/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/Utils/FileIO/FileWriter.h"
#include "Lib/Compressed/Matroska/Matroska.h"
#include "Lib/Uncompressed/HashSum/HashSum.h"
#include "Lib/Utils/FileIO/FileChecker.h"
extern "C"
{
#include "md5.h"
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
// Errors

namespace filewriter_issue {

namespace undecodable
{

static const char* MessageText[] =
{
    "cannot create directory",
    "cannot create file",
    "file already exists",
    "cannot write in the file",
    "cannot seek in the file",
};

enum code : uint8_t
{
    CreateDirectory,
    FileCreate,
    FileAlreadyExists,
    FileWrite,
    FileSeek,
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

} // filewriter_issue

//---------------------------------------------------------------------------
frame_writer::~frame_writer()
{
    delete (MD5_CTX*)MD5;
}

void frame_writer::FrameCall(raw_frame* RawFrame)
{
    if (!Mode[IsNotBegin])
    {
        if (!Mode[NoWrite])
        {
            // Open output file in writing mode only if the file does not exist
            file::return_value Result = File_Write.Open_WriteMode(BaseDirectory, OutputFileName, true);
            switch (Result)
            {
                case file::OK:
                case file::Error_FileAlreadyExists:
                    break;
                default:
                    if (Errors)
                        switch (Result)
                        {
                            case file::Error_CreateDirectory:       Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::CreateDirectory, OutputFileName); break;
                            case file::Error_FileCreate:            Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileCreate, OutputFileName); break;
                            default:                                Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName); break;
                        }
                    return;
            }
        }

        if (!Mode[NoOutputCheck] && !File_Write.IsOpen())
        {
            // File already exists, we want to check it
            if (File_Read.Open_ReadMode(BaseDirectory + OutputFileName))
            {
                Offset = (size_t)-1;
                SizeOnDisk = (size_t)-1;
                if (Errors)
                    Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::Frame_Source_Missing, OutputFileName);
                return;
            }
            SizeOnDisk = File_Read.Size();
        }
        else
            SizeOnDisk = (size_t)-1;
        Offset = 0;
    }

    // Do we need to write or check?
    if (Offset == (size_t)-1)
        return; // File is flagged as already with wrong data

    // Check hash operation
    if (M->Hashes || M->Hashes_FromRAWcooked || M->Hashes_FromAttachments)
    {
        if (!Mode[IsNotBegin])
        {
            if (!MD5)
                MD5 = new MD5_CTX;
            MD5_Init((MD5_CTX*)MD5);
        }

        CheckMD5(RawFrame);

        if (!Mode[IsNotEnd])
        {
            md5 MD5_Result;
            MD5_Final(MD5_Result.data(), (MD5_CTX*)MD5);

            if (M->Hashes)
                M->Hashes->FromFile(OutputFileName, MD5_Result);
            if (M->Hashes_FromRAWcooked)
                M->Hashes_FromRAWcooked->FromFile(OutputFileName, MD5_Result);
            if (M->Hashes_FromAttachments)
                M->Hashes_FromAttachments->FromFile(OutputFileName, MD5_Result);
        }
    }
                
    // Check file operation
    bool DataIsCheckedAndOk;
    if (File_Read.IsOpen())
    {
        if (!CheckFile(RawFrame))
        {
            if (Mode[IsNotEnd] || Offset == File_Read.Size())
                return; // All is OK
            DataIsCheckedAndOk = true;
        }
        else
            DataIsCheckedAndOk = false;

        // Files don't match, decide what we should do (overwrite or don't overwrite, log check error or don't log)
        File_Read.Close();
        bool HasNoError = false;
        if (!Mode[NoWrite] && UserMode)
        {
            user_mode NewMode = *UserMode;
            if (*UserMode == Ask && Ask_Callback)
            {
                NewMode = Ask_Callback(UserMode, OutputFileName, " and is not same", true, &M->ProgressIndicator_IsPaused, &M->ProgressIndicator_IsEnd);
            }
            if (NewMode == AlwaysYes)
            {
                if (file::return_value Result = File_Write.Open_WriteMode(BaseDirectory, OutputFileName, false))
                {
                    if (Errors)
                        switch (Result)
                        {
                        case file::Error_CreateDirectory:       Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::CreateDirectory, OutputFileName); break;
                        case file::Error_FileCreate:            Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileCreate, OutputFileName); break;
                        default:                                Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName); break;
                        }
                    Offset = (size_t)-1;
                    return;
                }
                if (Offset)
                {
                    if (File_Write.Seek(Offset))
                    {
                        if (Errors)
                            Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileSeek, OutputFileName);
                        Offset = (size_t)-1;
                        return;
                    }
                }

                // It is decided to overwrite the file, error "cancelled"
                HasNoError = true;
            }
        }
        if (!HasNoError)
        {
            if (Errors)
                Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::FileComparison, OutputFileName);
            Offset = (size_t)-1;
            return;
        }
    }
    else
        DataIsCheckedAndOk = false;

    // Write file operation
    if (File_Write.IsOpen())
    {
        // Write in the created file or file with wrong data being replaced
        if (!DataIsCheckedAndOk)
        {
            if (WriteFile(RawFrame))
            {
                if (Errors)
                    Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName);
                Offset = (size_t)-1;
                return;
            }
        }

        if (!Mode[IsNotEnd])
        {
            // Set end of file if necessary
            if (SizeOnDisk != (size_t)-1 && Offset != SizeOnDisk)
            {
                if (File_Write.SetEndOfFile())
                {
                    if (Errors)
                        Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName);
                    Offset = (size_t)-1;
                    return;
                }
            }

            // Close file
            if (File_Write.Close())
            {
                if (Errors)
                    Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName);
                Offset = (size_t)-1;
                return;
            }
        }

        return;
    }
}

//---------------------------------------------------------------------------
bool WriteFile_Write(size_t& Offset, file& File_Write, const buffer_base& Buffer)
{
    auto Buffer_Size = Buffer.Size();
    if (!Buffer_Size)
        return false;

    auto Buffer_Data = Buffer.Data();
    if (File_Write.Write(Buffer_Data, Buffer_Size))
        return true;

    Offset += Buffer_Size;
    return false;
}
bool frame_writer::WriteFile(raw_frame* RawFrame)
{
    if (WriteFile_Write(Offset, File_Write, RawFrame->Pre()))
        return true;
    if (WriteFile_Write(Offset, File_Write, RawFrame->Buffer()))
        return true;
    for (const auto& Plane : RawFrame->Planes())
        if (Plane && WriteFile_Write(Offset, File_Write, Plane->Buffer()))
            return true;
    if (WriteFile_Write(Offset, File_Write, RawFrame->Post()))
        return true;
    return false;
}

//---------------------------------------------------------------------------
bool CheckFile_Compare(size_t& Offset, const filemap& File, const buffer_base& Buffer)
{
    auto Buffer_Size = Buffer.Size();
    if (!Buffer_Size)
        return false;

    auto Buffer_Data = Buffer.Data();
    auto Offset_After = Offset + Buffer_Size;
    if (Offset_After > File.Size())
        return true;
    if (memcmp(File.Data() + Offset, Buffer_Data, Buffer_Size))
        return true;
    Offset = Offset_After;

    return false;
}
bool frame_writer::CheckFile(raw_frame* RawFrame)
{
    size_t Offset_Current = Offset;

    if (CheckFile_Compare(Offset_Current, File_Read, RawFrame->Pre()))
        return true;
    if (CheckFile_Compare(Offset_Current, File_Read, RawFrame->Buffer()))
        return true;
    for (const auto& Plane : RawFrame->Planes())
        if (Plane && CheckFile_Compare(Offset_Current, File_Read, Plane->Buffer()))
            return true;
    if (CheckFile_Compare(Offset_Current, File_Read, RawFrame->Post()))
        return true;

    Offset = Offset_Current;
    return false;
}

//---------------------------------------------------------------------------
bool frame_writer::CheckMD5(raw_frame* RawFrame)
{
    const auto Pre = RawFrame->Pre();
    if (Pre.Size())
        MD5_Update((MD5_CTX*)MD5, Pre.Data(), (unsigned long)Pre.Size());
    const auto Buffer = RawFrame->Buffer();
    if (Buffer.Size())
        MD5_Update((MD5_CTX*)MD5, Buffer.Data(), (unsigned long)Buffer.Size());
    for (const auto& Plane : RawFrame->Planes())
        if (Plane)
        {
            const auto& Buffer = Plane->Buffer();
            if (Buffer.Size())
                MD5_Update((MD5_CTX*)MD5, Buffer.Data(), (unsigned long)Buffer.Size());
        }
    const auto Post = RawFrame->Post();
    if (Post.Size())
        MD5_Update((MD5_CTX*)MD5, Post.Data(), (unsigned long)Post.Size());

    return false;
}

//---------------------------------------------------------------------------
