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
bool CheckFile_Compare(size_t& Offset, const filemap& File, const buffer_base& Buffer);

//---------------------------------------------------------------------------
frame_writer::~frame_writer()
{
    if (!Compound)
    {
        delete (MD5_CTX*)MD5;
        delete Output;
    }
}

//---------------------------------------------------------------------------
void frame_writer::FrameCall(raw_frame* RawFrame)
{
    if (!Output)
    {
        if (Compound)
        {
            Output = &Compound->Output;
            if (Output->Write.IsOpen() || Output->Read.IsOpen())
            {
                Mode[frame_writer::IsNotBegin] = true;
                Mode[frame_writer::IsNotEnd] = true;
            }
            MD5 = Compound->MD5;
        }
        else
        {
            Output = new file_output;
        }
    }

    if (!Mode[IsNotBegin])
    {
        if (!Mode[NoWrite])
        {
            // Open output file in writing mode only if the file does not exist
            file::return_value Result = Output->Write.Open_WriteMode(BaseDirectory, OutputFileName, true);
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

        Output->Offset = 0;
        if (!Mode[NoOutputCheck] && !Output->Write.IsOpen())
        {
            // File already exists, we want to check it
            if (Output->Read.Open_ReadMode(BaseDirectory + OutputFileName))
            {
                Output->Offset = (size_t)-1;
                SizeOnDisk = (size_t)-1;
                if (Errors)
                    Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::Frame_Source_Missing, OutputFileName);
                return;
            }
            SizeOnDisk = Output->Read.Size();
            if (Compound)
            {
                // Do we need to write or check?
                if (Output->Offset == (size_t)-1)
                    return; // File is flagged as already with wrong data

                if (!Mode[NoOutputCheck])
                {
                    if (CheckFile_Compare(Output->Offset, Output->Read, buffer_view(Compound->Input.Data(), Compound->Positions[0].Input_Offset))) // Output->Write.Write(Compound->Input.Data(), Compound->Positions[0].Input_Offset);
                    {
                        if (Errors)
                            Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::FileComparison, OutputFileName);
                        Output->Offset = (size_t)-1;
                        return;
                    }
                }
                Mode[frame_writer::IsNotBegin] = true;
                Mode[frame_writer::IsNotEnd] = true;
            }
        }
        else
        {
            SizeOnDisk = (size_t)-1;
            if (Compound)
            {
                if (!Mode[NoOutputCheck])
                    Output->Write.Write(Compound->Input.Data(), Compound->Positions[0].Input_Offset);
                Mode[frame_writer::IsNotBegin] = true;
                Mode[frame_writer::IsNotEnd] = true;
            }
        }
        if (Compound)
        {
            // Check hash operation
            if ((M->Hashes && M->Hashes->HashFiles_Count()) || M->Hashes_FromRAWcooked || M->Hashes_FromAttachments)
            {
                if (!MD5)
                {
                    if (!Compound->MD5)
                        Compound->MD5 = new MD5_CTX;
                    MD5 = Compound->MD5;
                }
                MD5_Init((MD5_CTX*)MD5);
                MD5_Update((MD5_CTX*)MD5, Compound->Input.Data(), (unsigned long)Compound->Positions[0].Input_Offset);
                Mode[frame_writer::IsNotBegin] = true;
                Mode[frame_writer::IsNotEnd] = true;
            }
        }
    }

    // Do we need to write or check?
    if (Output->Offset == (size_t)-1)
        return; // File is flagged as already with wrong data

    // Check hash operation
    if ((M->Hashes && M->Hashes->HashFiles_Count()) || M->Hashes_FromRAWcooked || M->Hashes_FromAttachments)
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

            if (M->Hashes && M->Hashes->HashFiles_Count())
                M->Hashes->FromFile(OutputFileName, MD5_Result);
            if (M->Hashes_FromRAWcooked)
                M->Hashes_FromRAWcooked->FromFile(OutputFileName, MD5_Result);
            if (M->Hashes_FromAttachments)
                M->Hashes_FromAttachments->FromFile(OutputFileName, MD5_Result);
        }
    }
                
    // Check file operation
    bool DataIsCheckedAndOk;
    if (Output->Read.IsOpen())
    {
        if (!CheckFile(RawFrame))
        {
            if (Mode[IsNotEnd] || Output->Offset == Output->Read.Size())
                return; // All is OK
            DataIsCheckedAndOk = true;
        }
        else
            DataIsCheckedAndOk = false;

        // Files don't match, decide what we should do (overwrite or don't overwrite, log check error or don't log)
        Output->Read.Close();
        if (Compound)
        {
            if (Errors)
                Errors->Error(IO_FileChecker, error::type::Undecodable, (error::generic::code)filechecker_issue::undecodable::FileComparison, OutputFileName);
            Output->Offset = (size_t)-1;
            return;
        }
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
                if (file::return_value Result = Output->Write.Open_WriteMode(BaseDirectory, OutputFileName, false))
                {
                    if (Errors)
                        switch (Result)
                        {
                        case file::Error_CreateDirectory:       Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::CreateDirectory, OutputFileName); break;
                        case file::Error_FileCreate:            Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileCreate, OutputFileName); break;
                        default:                                Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName); break;
                        }
                    Output->Offset = (size_t)-1;
                    return;
                }
                if (Output->Offset)
                {
                    if (Output->Write.Seek(Output->Offset))
                    {
                        if (Errors)
                            Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileSeek, OutputFileName);
                        Output->Offset = (size_t)-1;
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
            Output->Offset = (size_t)-1;
            return;
        }
    }
    else
        DataIsCheckedAndOk = false;

    // Write file operation
    if (Output->Write.IsOpen())
    {
        // Write in the created file or file with wrong data being replaced
        if (!DataIsCheckedAndOk)
        {
            if (WriteFile(RawFrame))
            {
                if (Errors)
                    Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName);
                Output->Offset = (size_t)-1;
                return;
            }
        }

        if (!Mode[IsNotEnd])
        {
            // Set end of file if necessary
            if (SizeOnDisk != (size_t)-1 && Output->Offset != SizeOnDisk)
            {
                if (Output->Write.SetEndOfFile())
                {
                    if (Errors)
                        Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName);
                    Output->Offset = (size_t)-1;
                    return;
                }
            }

            // Close file
            if (Output->Write.Close())
            {
                if (Errors)
                    Errors->Error(IO_FileWriter, error::type::Undecodable, (error::generic::code)filewriter_issue::undecodable::FileWrite, OutputFileName);
                Output->Offset = (size_t)-1;
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
    if (Compound)
    {
        buffer_view ToWrite;
        uint16_t Index;
        size_t* Positions_StreamOffsetTemp;
        size_t* AdditionalBytesTemp;
        size_t AdditionalBytesTemp_Fake = 0;
        if (RawFrame->Buffer().Size() == 0) //TODO
        {
            ToWrite = RawFrame->Plane(0)->Buffer();
            Index = 0;
            Positions_StreamOffsetTemp = &Compound->Positions_Offset_Video;
            AdditionalBytesTemp = &AdditionalBytesTemp_Fake;
        }
        else
        {
            ToWrite = RawFrame->Buffer();
            Index = 1;
            Positions_StreamOffsetTemp = &Compound->Positions_Offset_Audio;
            AdditionalBytesTemp = &Compound->Positions_Offset_Audio_AdditionalBytes;
        }
        size_t& Positions_StreamOffset = *Positions_StreamOffsetTemp;
        size_t& AdditionalBytes = *AdditionalBytesTemp;
        size_t SizeToWrite = ToWrite.Size();
        size_t SizeWritten = 0;
        while (SizeToWrite)
        {
            // Find the next frame
            while (Compound->Positions[Positions_StreamOffset].Index != Index)
                Positions_StreamOffset++;

            auto SizeOfFrame = Compound->Positions[Positions_StreamOffset].Size - AdditionalBytes;
            if (SizeToWrite < SizeOfFrame || Positions_StreamOffset != Compound->Positions_Offset_InFileWritten)
            {
                // Store the content for later
                if (!Compound->Positions[Positions_StreamOffset].Buffer)
                {
                    Compound->Positions[Positions_StreamOffset].Buffer = new buffer();
                    Compound->Positions[Positions_StreamOffset].Buffer->Create(SizeOfFrame);
                }
                auto SizeToWriteInThisFrame = SizeToWrite >= SizeOfFrame ? SizeOfFrame : SizeToWrite;
                memcpy(Compound->Positions[Positions_StreamOffset].Buffer->Data() + AdditionalBytes, ToWrite.Data() + SizeWritten, SizeToWriteInThisFrame);
                SizeWritten += SizeToWriteInThisFrame;

                if (SizeToWrite < SizeOfFrame)
                {
                    // Still need the frame
                    AdditionalBytes += SizeToWrite;
                    SizeToWrite = 0;
                }
                else
                {
                    AdditionalBytes = 0;
                    SizeToWrite -= SizeOfFrame;

                    // Find the next frame
                    Positions_StreamOffset++;
                    while (Positions_StreamOffset < Compound->Positions.size() && Compound->Positions[Positions_StreamOffset].Index != Index)
                        Positions_StreamOffset++;
                }
            }
            else
            {
                if (AdditionalBytes)
                {
                    auto& Buffer_Before = Compound->Positions[Compound->Positions_Offset_InFileWritten].Buffer;
                    if (Output->Write.Write(Buffer_Before->Data(), AdditionalBytes))
                        return true;
                    delete Buffer_Before;
                    Buffer_Before = nullptr;
                }
                if (Output->Write.Write(ToWrite.Data() + SizeWritten, SizeOfFrame))
                    return true;
                SizeWritten += SizeOfFrame;
                AdditionalBytes = 0;
                SizeToWrite -= SizeOfFrame;

                // Find the next frame
                Positions_StreamOffset++;
                while (Positions_StreamOffset < Compound->Positions.size() && Compound->Positions[Positions_StreamOffset].Index != Index)
                    Positions_StreamOffset++;

                do
                {
                    auto& Buffer_Before = Compound->Positions[Compound->Positions_Offset_InFileWritten].Buffer;
                    if (Buffer_Before && Buffer_Before->Size() != 0)
                    {
                        if (Output->Write.Write(Buffer_Before->Data(), Buffer_Before->Size()))
                            return true;
                        delete Buffer_Before;
                        Buffer_Before = nullptr;
                    }

                    uint64_t Input_Offset;
                    Compound->Positions_Offset_InFileWritten++;
                    if (Compound->Positions_Offset_InFileWritten == Compound->Positions.size())
                    {
                        Input_Offset = Compound->Input.Size();
                    }
                    else
                    {
                        Input_Offset = Compound->Positions[Compound->Positions_Offset_InFileWritten].Input_Offset;
                    }
                    auto Size = Input_Offset - Compound->Positions[Compound->Positions_Offset_InFileWritten - 1].Input_Offset;
                    if (Output->Write.Write(Compound->Input.Data() + Input_Offset - Size, Size))
                        return true;
                } while (Compound->Positions_Offset_InFileWritten < Compound->Positions.size() && Compound->Positions_Offset_InFileWritten < Compound->Positions_Offset_Video && Compound->Positions_Offset_InFileWritten < Compound->Positions_Offset_Audio);
            }
        }

        return false;
    }
    
    if (WriteFile_Write(Output->Offset, Output->Write, RawFrame->Pre()))
        return true;
    if (WriteFile_Write(Output->Offset, Output->Write, RawFrame->Buffer()))
        return true;
    for (const auto& Plane : RawFrame->Planes())
        if (Plane && WriteFile_Write(Output->Offset, Output->Write, Plane->Buffer()))
            return true;
    if (WriteFile_Write(Output->Offset, Output->Write, RawFrame->Post()))
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
    if (Compound)
    {
        buffer_view ToWrite;
        uint16_t Index;
        size_t* Positions_StreamOffsetTemp;
        size_t* AdditionalBytesTemp;
        size_t AdditionalBytesTemp_Fake = 0;
        if (RawFrame->Buffer().Size() == 0) //TODO
        {
            ToWrite = RawFrame->Plane(0)->Buffer();
            Index = 0;
            Positions_StreamOffsetTemp = &Compound->Positions_Offset_Video;
            AdditionalBytesTemp = &AdditionalBytesTemp_Fake;
        }
        else
        {
            ToWrite = RawFrame->Buffer();
            Index = 1;
            Positions_StreamOffsetTemp = &Compound->Positions_Offset_Audio;
            AdditionalBytesTemp = &Compound->Positions_Offset_Audio_AdditionalBytes;
        }
        size_t& Positions_StreamOffset = *Positions_StreamOffsetTemp;
        size_t& AdditionalBytes = *AdditionalBytesTemp;
        size_t SizeToWrite = ToWrite.Size();
        size_t SizeWritten = 0;
        while (SizeToWrite)
        {
            // Find the next frame
            while (Compound->Positions[Positions_StreamOffset].Index != Index)
                Positions_StreamOffset++;

            auto SizeOfFrame = Compound->Positions[Positions_StreamOffset].Size - AdditionalBytes;
            if (SizeToWrite < SizeOfFrame || Positions_StreamOffset != Compound->Positions_Offset_InFileWritten)
            {
                // Store the content for later
                if (!Compound->Positions[Positions_StreamOffset].Buffer)
                {
                    Compound->Positions[Positions_StreamOffset].Buffer = new buffer();
                    Compound->Positions[Positions_StreamOffset].Buffer->Create(SizeOfFrame);
                }
                auto SizeToWriteInThisFrame = SizeToWrite >= SizeOfFrame ? SizeOfFrame : SizeToWrite;
                memcpy(Compound->Positions[Positions_StreamOffset].Buffer->Data() + AdditionalBytes, ToWrite.Data() + SizeWritten, SizeToWriteInThisFrame);
                SizeWritten += SizeToWriteInThisFrame;

                if (SizeToWrite < SizeOfFrame)
                {
                    // Still need the frame
                    AdditionalBytes += SizeToWrite;
                    SizeToWrite = 0;
                }
                else
                {
                    AdditionalBytes = 0;
                    SizeToWrite -= SizeOfFrame;

                    // Find the next frame
                    Positions_StreamOffset++;
                    while (Positions_StreamOffset < Compound->Positions.size() && Compound->Positions[Positions_StreamOffset].Index != Index)
                        Positions_StreamOffset++;
                }
            }
            else
            {
                if (AdditionalBytes)
                {
                    auto& Buffer_Before = Compound->Positions[Compound->Positions_Offset_InFileWritten].Buffer;
                    if (CheckFile_Compare(Output->Offset, Output->Read, buffer_view(Buffer_Before->Data(), AdditionalBytes))) // Output->Write.Write(Buffer_Before->Data(), AdditionalBytes);
                        return true;
                    delete Buffer_Before;
                    Buffer_Before = nullptr;
                }
                if (CheckFile_Compare(Output->Offset, Output->Read, buffer_view(ToWrite.Data() + SizeWritten, SizeOfFrame))) // Output->Write.Write(ToWrite.Data() + SizeWritten, SizeOfFrame))
                    return true;
                SizeWritten += SizeOfFrame;
                AdditionalBytes = 0;
                SizeToWrite -= SizeOfFrame;

                // Find the next frame
                Positions_StreamOffset++;
                while (Positions_StreamOffset < Compound->Positions.size() && Compound->Positions[Positions_StreamOffset].Index != Index)
                    Positions_StreamOffset++;

                do
                {
                    auto& Buffer_Before = Compound->Positions[Compound->Positions_Offset_InFileWritten].Buffer;
                    if (Buffer_Before && Buffer_Before->Size() != 0)
                    {
                        if (CheckFile_Compare(Output->Offset, Output->Read, *Buffer_Before)) // Output->Write.Write(Buffer_Before->Data(), Buffer_Before->Size());
                            return true;
                        delete Buffer_Before;
                        Buffer_Before = nullptr;
                    }

                    uint64_t Input_Offset;
                    Compound->Positions_Offset_InFileWritten++;
                    if (Compound->Positions_Offset_InFileWritten == Compound->Positions.size())
                    {
                        Input_Offset = Compound->Input.Size();
                    }
                    else
                    {
                        Input_Offset = Compound->Positions[Compound->Positions_Offset_InFileWritten].Input_Offset;
                    }
                    auto Size = Input_Offset - Compound->Positions[Compound->Positions_Offset_InFileWritten - 1].Input_Offset;
                    if (CheckFile_Compare(Output->Offset, Output->Read, buffer_view(Compound->Input.Data() + Input_Offset - Size, Size))) // Output->Write.Write(Compound->Input.Data() + Input_Offset - Size, Size);
                        return true;
                } while (Compound->Positions_Offset_InFileWritten < Compound->Positions.size() && Compound->Positions_Offset_InFileWritten < Compound->Positions_Offset_Video && Compound->Positions_Offset_InFileWritten < Compound->Positions_Offset_Audio);
            }
        }

        return false;
    }

    size_t Offset_Current = Output->Offset;

    if (CheckFile_Compare(Offset_Current, Output->Read, RawFrame->Pre()))
        return true;
    if (CheckFile_Compare(Offset_Current, Output->Read, RawFrame->Buffer()))
        return true;
    for (const auto& Plane : RawFrame->Planes())
        if (Plane && CheckFile_Compare(Offset_Current, Output->Read, Plane->Buffer()))
            return true;
    if (CheckFile_Compare(Offset_Current, Output->Read, RawFrame->Post()))
        return true;

    Output->Offset = Offset_Current;
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
