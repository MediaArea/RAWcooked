/*  Copyright (c) MediaArea.net SARL & Reto Kromer.
 *
 *  Use of this source code is governed by a BSD-style license that can
 *  be found in the License.html file in the root of the source tree.
 */

//---------------------------------------------------------------------------
#include "Lib/License/License.h"
#include "Lib/License/License_Internal.h"
#include "Lib/Utils/FileIO/Input_Base.h"
#define __STDC_WANT_LIB_EXT1__ 1
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <iomanip>
#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
    #include <Shlobj.h>
    #define PathSeparator "\\"
#else
    #include <sys/stat.h>
    #define PathSeparator "/"
#endif
using namespace std;
//---------------------------------------------------------------------------

//***************************************************************************
// Secure version of some OS calls
//***************************************************************************

//---------------------------------------------------------------------------
static string getenv_string(const char* name)
{
    #if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__)) // TODO: getenv_s is supported with GCC only with C++17.
        // Get size
        size_t len;
        if (getenv_s(&len, NULL, 0, name) || !len)
            return string();

        // Get value
        char* value = new char[len * sizeof(char)];
        if (getenv_s(&len, value, len, name))
            return string();

        // Create string
        string valueString(value, len);
        delete[] value;
        return valueString;
    #else
        // FIXME: this is not secure
        //struct _reent Reent;
        //return string(&Reent, getenv_s(name));
        const char* ToReturn = getenv(name);
        if (ToReturn)
            return ToReturn;
        return string();
    #endif
}

//---------------------------------------------------------------------------
static tm localtime_tm(const time_t& time)
{
    tm result;
    #if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
        localtime_s(&result, &time);
    #else
        localtime_r(&time, &result);
    #endif

    return result;
}

//---------------------------------------------------------------------------
bool CreateDirectory (const string& Dir)
{
    #if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
        DWORD dwAttrib = GetFileAttributesA(Dir.c_str());
        if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
            return false;
        if (CreateDirectoryA(Dir.c_str(), NULL))
            return false;
        #else
        struct stat s = {};
        if (!stat(Dir.c_str(), &s) && (s.st_mode & S_IFDIR))
            return false;
        if (!mkdir(Dir.c_str(), 0700))
            return false;
    #endif

    cerr << "Problem while creating " << Dir << " directory" << endl;
        
    return true;
}

//---------------------------------------------------------------------------
string GetLocalConfigPath (bool Create = false)
{
    string result;
    #if (defined(WIN32) || defined(_WIN32) || defined(__WIN32__))
        char szPath[MAX_PATH];
        if(SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, szPath)))
            result = szPath;
    #else
        result = getenv_string("HOME");
        if (!result.empty())
            result += '/';
        result += ".config";
    #endif

    if (!result.empty())
    {
        if (Create)
            CreateDirectory(result);
        result += PathSeparator "RAWcooked";
        if (Create)
            CreateDirectory(result);
    }
    return result;
}

//***************************************************************************
// License input
//***************************************************************************

//---------------------------------------------------------------------------
class license_input : public input_base
{
public:
    license_input() : input_base(Parser_License) {}

    license_internal*           License = nullptr;

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
};

//---------------------------------------------------------------------------
void license_input::BufferOverflow()
{
}

//***************************************************************************
// Place holder for license encoding/decoding
//***************************************************************************

//---------------------------------------------------------------------------
// Place holder for license key related functions - Begin
// Place holder for license key related functions - End

//---------------------------------------------------------------------------
buffer license_internal::ToBuffer()
{
    buffer Buffer;

    // Place holder for license key encoding - Begin
    // Place holder for license key encoding - End

    return Buffer;
}

//---------------------------------------------------------------------------
void license_input::ParseBuffer()
{
    // Place holder for license key decoding - Begin
    // Place holder for license key decoding - End
}

//***************************************************************************
// license_internal
//***************************************************************************

//---------------------------------------------------------------------------
license_internal::license_internal(bool IgnoreDefault)
{
    // Default license flags
    if (!IgnoreDefault)
        for (size_t i = 0; DefaultLicense_Parsers[i].Value != (uint8_t)-1; i++)
            SetSupported(DefaultLicense_Parsers[i].Value, DefaultLicense_Parsers[i].Flavor);
}

size_t license_internal::Flags_Pos_Get(uint8_t Type, uint8_t SubType)
{
    size_t Flags_Pos = 0;
    for (size_t i = 0; i < Type; i++)
        Flags_Pos += License_Infos[i].Size;
    Flags_Pos += SubType;

    if (Flags_Pos >= Flags_.size())
        Flags_.resize(Flags_Pos + 1);

    return Flags_Pos;
}

void license_internal::SetSupported(uint8_t Type, uint8_t SubType)
{
    Flags_[Flags_Pos_Get(Type, SubType)] = true;
}

bool license_internal::IsSupported(uint8_t Type, uint8_t SubType)
{
    return Flags_[Flags_Pos_Get(Type, SubType)];
}

//---------------------------------------------------------------------------
bool license_internal::FromString(const string& FromUser)
{
    // SizeOnDisk
    if (FromUser.size() > 200)
    {
        cerr << "Invalid size of license key." << endl;
        return true;
    }

    // Analyze
    uint8_t Buffer[100];
    size_t Buffer_Offset = 0;
    for (; Buffer_Offset < FromUser.size() / 2; Buffer_Offset++)
    {
        const char L1 = FromUser[Buffer_Offset * 2];
        const char L2 = FromUser[Buffer_Offset * 2 + 1];
        if (!((L1 >= '0' && L1 <= '9') || (L1 >= 'A' && L1 <= 'F'))
            && !((L2 >= '0' && L2 <= '9') || (L2 >= 'A' && L2 <= 'F')))
        {
            cerr << "Invalid characters in license key." << endl;
            return true;
        }

        Buffer[Buffer_Offset] = ((L1 - ((L1 <= '9') ? '0' : ('A' - 10))) << 4)
            | ((L2 - ((L2 <= '9') ? '0' : ('A' - 10))));
    }

    // Parse
    return FromBuffer(buffer_view(Buffer, Buffer_Offset));
}

//---------------------------------------------------------------------------
bool license_internal::FromBuffer(const buffer_view& Buffer)
{
    license_input Input;
    Input.License = this;
    if (Input.Parse(Buffer))
    {
        if (!Input.IsSupported())
        {
            cerr << "License detected but decoding not supported in this version." << endl;
            *this = license_internal();
            return false;
        }
        // TEMP IsSupported !License
        return true;
    }
    return false;
}

//---------------------------------------------------------------------------
string license_internal::ToString()
{
    // To string
    auto Buffer = ToBuffer();
    string ToReturn;
    ToReturn.reserve(Buffer.Size());
    for (size_t i = 0; i < Buffer.Size(); i++)
    {
        uint8_t Value = Buffer[i];
        Value >>= 4;
        Value += (Value < 10 ? '0' : ('A' - 10));
        ToReturn.push_back((char)Value);
        Value = Buffer[i];
        Value &= 0xF;
        Value += (Value < 10 ? '0' : ('A' - 10));
        ToReturn.push_back((char)Value);
    }
    return ToReturn;
}

//***************************************************************************
// License class
//***************************************************************************

//---------------------------------------------------------------------------
license::license()
{
    Internal = new license_internal();
}

//---------------------------------------------------------------------------
license::~license()
{
    delete (license_internal*)Internal;
}

//---------------------------------------------------------------------------
void license::Feature(feature Value) { license_internal* License = (license_internal*)Internal; License->Input_Flags[0] |= ((uint64_t)1 << (int)Value); }
void license::Muxer(muxer Value) { license_internal* License = (license_internal*)Internal; License->Input_Flags[1] |= ((uint64_t)1 << (int)Value); }
void license::Encoder(encoder Value) { license_internal* License = (license_internal*)Internal; License->Input_Flags[2] |= ((uint64_t)1 << (int)Value); }

//---------------------------------------------------------------------------
bool license::LoadLicense(string LicenseKey, bool StoreLicenseKey)
{
    // Read license from environment variable
    if (LicenseKey.empty())
        LicenseKey = getenv_string("RAWcooked_License");

    // Read license from a file
    if (LicenseKey.empty())
    {
        string Path = GetLocalConfigPath() + PathSeparator "Config.txt";
        ifstream F(Path);
        string temp;
        while (!F.fail() && !F.eof())
        {
            F >> temp;
            if (temp.rfind("License=", 8) == 0 || temp.rfind("Licence=", 8) == 0)
                LicenseKey = temp.substr(8);
        }
    }

    // Decode
    if (!LicenseKey.empty())
    {
        license_internal* License = (license_internal*)Internal;
        if (License->FromString(LicenseKey))
            return true;
        
        if (StoreLicenseKey)
        {
            string Path = GetLocalConfigPath(true);
            Path += PathSeparator "Config.txt";
            fstream F(Path, std::fstream::in | std::fstream::out);
            if (!F.is_open())
            {
                F.open(Path, ios_base::app);
                if (!F.is_open())
                {
                    cerr << "Can not open " << Path << '.' << endl;
                    return true;
                }
            }
            else
            {
                F.seekp(-1, ios_base::end);
                if (!F.fail())
                {
                    char temp;
                    F.read(&temp, 1);
                    if (F.fail())
                    {
                        cerr << "Can not read in " << Path << '.' << endl;
                        return true;
                    }
                    if (!F.fail() && !F.eof() && temp != '\n')
                    {
                        F.seekp(0, ios_base::end);
                        F.write("\n", 1); // endl;
                        if (F.fail())
                        {
                            cerr << "Can not write in " << Path << '.' << endl;
                            return true;
                        }
                    }
                    else
                        F.seekp(0, ios_base::end);
                }
            }
            F.write(string("License=" + LicenseKey + '\n').c_str(), 9 + LicenseKey.size());
            if (F.fail())
            {
                cerr << "Can not write in " << Path << '.' << endl;
                return true;
            }
        }
    }

    return false;
}

//---------------------------------------------------------------------------
bool license::ShowLicense(bool Verbose, uint64_t NewSublicenseId, uint64_t NewSubLicenseDur)
{
    license_internal* License = (license_internal*)Internal;

    // UserID
    if (License->OwnerID)
    {
        if (Verbose)
        {
            cerr << "License owner ID = " << License->OwnerID;
            if (License->SubLicenseeID)
                cerr << " (sub-licensee ID = " << License->SubLicenseeID << ')';
            cerr << '.';
            if (License->Date == (time_t)-1)
                cerr << '\n' << endl;
            else
                cerr << ' ';
        }
    }
    else if (License->Date == (time_t)-1)
        cerr << "Using default license.\nConsider to support RAWcooked: https://MediaArea.net/RAWcooked\n" << endl;

    // Date
    if (License->Date != (time_t)-1)
    {
        struct tm TimeInfo = localtime_tm(License->Date);
        cerr << "Temporary license, valid until " << TimeInfo.tm_year + 1900 << '-' << setfill('0') << setw(2) << TimeInfo.tm_mon + 1 << '-' << setfill('0') << setw(2) << TimeInfo.tm_mday << ".\n" ;

        time_t Time;
        time(&Time);
        if (Time > License->Date)
        {
            cerr << "Outdated license, using default license.\n";
            auto SubLicenseeID_Sav = License->SubLicenseeID;
            *License = license_internal();
            License->SubLicenseeID = SubLicenseeID_Sav;
        }

        cerr << endl;
    }

    if (NewSublicenseId)
    {
        if (License->SubLicenseeID)
        {
            cerr << "Error: a sublicensee can not sublicense.\n";
            return true;
        }
        License->SubLicenseeID = NewSublicenseId;
        License->SetDateFromMonths((int)NewSubLicenseDur);

        cerr << "Sub-licensee license key: ";
        cout << License->ToString();
        cerr << endl;
    }

    if (!Verbose)
        return false;

    // Info
    for (int8_t i = 0; i < License_Infos_Size; i++)
    {
        cerr << "Licensed " << License_Infos[i].Name << ':' << endl;
        for (int8_t j = 0; j < License_Infos[i].Size; j++)
        {
            const char* Supported = License->IsSupported(i, j) ? "Yes" : "No ";
            cerr << Supported << ' ' << License_Infos[i].Flavor_String(j) << endl;
        }
        cerr << endl;
    }

    return false;
}

//---------------------------------------------------------------------------
bool license::IsSupported()
{
    license_internal* License = (license_internal*)Internal;

    // Info
    for (int8_t i = 0; i < License_Parser_Offset; i++)
        for (int8_t j = 0; j < License_Infos[i].Size; j++)
            if (License->Input_Flags[i] & ((uint64_t)1 << j) && !License->IsSupported(i, j))
                return false;
    
    return true;
}

//---------------------------------------------------------------------------
bool license::IsSupported(feature Feature)
{
    license_internal* License = (license_internal*)Internal;

    return License->IsSupported(0, (uint8_t)Feature);
}

//---------------------------------------------------------------------------
bool license::IsSupported(parser Parser, uint8_t Flavor)
{
    license_internal* License = (license_internal*)Internal;

    return License->IsSupported(Parser, Flavor);
}

//---------------------------------------------------------------------------
bool license::IsSupported_License()
{
    license_input Input;
    buffer Buffer;
    return !Input.Parse(Buffer) && Input.IsSupported();
}

//---------------------------------------------------------------------------
void license_internal::SetDateFromMonths(int Months)
{
    time_t Time;
    time(&Time);
    struct tm TimeInfo = localtime_tm(Time);
    TimeInfo.tm_mon += (decltype(TimeInfo.tm_mon))Months;
    Time = mktime(&TimeInfo);
    if (Date < 0 || Date > Time)
        Date = Time;
}
