/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#ifndef FileIOH
#define FileIOH
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "Lib/Utils/Errors/Errors.h"
#include "Lib/Utils/RawFrame/RawFrame.h"
using namespace std;
//---------------------------------------------------------------------------

class filemap : public buffer_view
{
public:
    // Constructor/Destructor
                                filemap() : buffer_view(nullptr, 0) {}
                                ~filemap() { Close(); }

    // Actions
    enum class method
    {
        mmap,
        fstream,
        fopen,
        open,
        #if defined(_WIN32) || defined(_WINDOWS)
        createfile,
        #endif //defined(_WIN32) || defined(_WINDOWS)
    };
    int                         Open_ReadMode(const char* FileName, method NewMethod = {}, size_t Begin = {}, size_t End = {});
    int                         Open_ReadMode(const string& FileName, method NewMethod = {}, size_t Begin = {}, size_t End = {}) { return Open_ReadMode(FileName.c_str(), NewMethod, Begin, End); }
    bool                        IsOpen() { return Private == (decltype(Private))-1 ? false : true; }
    int                         Remap(size_t Begin = 0, size_t End = 0);
    int                         Close();

private:
    #if defined(_WIN32) || defined(_WINDOWS)
    void*                       Private = (void*)-1;
    #else //defined(_WIN32) || defined(_WINDOWS)
    int                         Private = (int)-1;
    #endif //defined(_WIN32) || defined(_WINDOWS)
    void*                       Private2 = (void*)-1;
    method                      Method = {};
};

class file
{
public:
    // Constructor/Destructor
                                ~file() { Close(); }

    // Actions
    enum return_value
    {
        OK = 0,
        Error_CreateDirectory,
        Error_FileCreate,
        Error_FileAlreadyExists,
        Error_FileWrite,
        Error_Seek,
    };
    return_value                Open_WriteMode(const string& BaseDirectory_Source, const string& OutputFileName_Source, bool RejectIfExists = false, bool Truncate = false);
    bool                        IsOpen() { return Private == (void*)-1 ? false : true; }
    return_value                Write(const uint8_t* Buffer, size_t Size);
    return_value                Write(const buffer_base& Buffer) { return Write(Buffer.Data(), Buffer.Size()); }
    enum seek_value
    {
        Begin,
        Current,
        End,
    };
    return_value                Seek(int64_t Offset, seek_value Method = Begin);
    return_value                SetEndOfFile();
    return_value                Close();

private:
    void*                       Private = (void*)-1;
    string                      OutputFileName;
};

#endif
