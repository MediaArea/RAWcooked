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

#if defined(_WIN32) || defined(_WINDOWS)
// File name for the wide API, with the \\?\ prefix if the full path is too long for the Win32 API (MAX_PATH)
wstring WideFileName(const char* FileName);
inline wstring WideFileName(const string& FileName) { return WideFileName(FileName.c_str()); }
// File name from the wide API, in the code page of the ANSI API
string NarrowFileName(const wchar_t* FileName);
#endif //defined(_WIN32) || defined(_WINDOWS)

class filemap : public buffer_view
{
public:
    // Constructor/Destructor
                                filemap() : buffer_view(nullptr, 0) {}
                                ~filemap() { Close(); delete[] Header_Buffer; }

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
        header, // Internal, set by Open_HeaderMode()
    };
    int                         Open_ReadMode(const char* FileName, uintmax_t FileSize = 0, method NewMethod = {}, size_t Begin = {}, size_t End = {}, bool AlsoWtite_ = false);
    int                         Open_ReadMode(const string& FileName, uintmax_t FileSize = 0, method NewMethod = {}, size_t Begin = {}, size_t End = {}, bool AlsoWrite_ = false) { return Open_ReadMode(FileName.c_str(), FileSize, NewMethod, Begin, End, AlsoWrite_); }
    // Reads only the first HeaderSize bytes, without memory map (cheaper when only the header is needed, e.g. for editing it): Size() is the file size, Readable_End() the count of bytes read
    int                         Open_HeaderMode(const string& FileName, size_t HeaderSize, bool AlsoWrite_ = false, uintmax_t FileSize = 0);
    // Writes the bytes from Begin to End of Data() to the file (needed only if the file is not memory mapped)
    int                         Write(size_t Begin, size_t End);
    bool                        IsOpen() { return Private == (decltype(Private))-1 ? false : true; }
    int                         Remap(size_t Begin = 0, size_t End = 0);
    int                         Close();

    // Memory map kept until released, also after a Remap() or a Close() (memory map method only, else nullptr)
    void*                       Hold();
    static void                 Release(void* Map);

    // End of the bytes readable from Data(), offset in the file (more than Size() if bytes after the end of the file are readable)
    size_t                      Readable_End();

private:
    #if defined(_WIN32) || defined(_WINDOWS)
    void*                       Private = (void*)-1;
    #else //defined(_WIN32) || defined(_WINDOWS)
    int                         Private = (int)-1;
    #endif //defined(_WIN32) || defined(_WINDOWS)
    void*                       Private2 = (void*)-1;
    method                      Method = {};
    bool                        AlsoWrite = false;
    void*                       Map = nullptr; // Memory map, with the count of its users
    size_t                      Header_Size = 0; // Count of bytes read in header mode
    uint8_t*                    Header_Buffer = nullptr; // Buffer for header mode, kept for the next files
    size_t                      Header_Buffer_Size = 0;
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
