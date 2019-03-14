/*  Copyright (c) MediaArea.net SARL & AV Preservation by reto.ch.
*
*  Use of this source code is governed by a BSD-style license that can
*  be found in the License.html file in the root of the source tree.
*/

//---------------------------------------------------------------------------
#include "Lib/License.h"
#include "Lib/License_Internal.h"
#include "Lib/Input_Base.h"
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
        struct stat s = { 0 };
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
// License decode
//***************************************************************************

//---------------------------------------------------------------------------
class license_input : public input_base
{
public:
    license_input() : input_base(Parser_License), Supported(false) {}

    license_internal* License;
    bool Supported;

private:
    void                        ParseBuffer();
    void                        BufferOverflow();
};

//---------------------------------------------------------------------------
void license_input::ParseBuffer()
{
    // Place holder for license key descrambling - Begin
    // Place holder for license key descrambling - End
}

//---------------------------------------------------------------------------
void license_input::BufferOverflow()
{
}

//---------------------------------------------------------------------------
bool DecodeLicense(const string& FromUser, license_internal* License)
{
    // Size
    if (FromUser.size()>200)
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
    license_input Input;
    Input.License = License;
    if (Input.Parse(Buffer, Buffer_Offset))
    {
        if (License && !Input.Supported)
        {
            cerr << "License detected but decoding not supported in this version." << endl;
            *License = license_internal();
            return false;
        }
        if (!License)
            return !Input.Supported;
        return true;
    }
    return false;
}

//***************************************************************************
// license_internal
//***************************************************************************

//---------------------------------------------------------------------------
license_internal::license_internal()
    : Date((time_t)-1)
    , UserID(0)
{
    memset(License_Flags, 0x00, sizeof(License_Flags));
    memset(Input_Flags, 0x00, sizeof(Input_Flags));

    // Default license flags
    for (size_t i = 0; DefaultLicense_Parsers[i].Value != (uint8_t)-1; i++)
        AddFlavor(License_Flags[DefaultLicense_Parsers[i].Value], DefaultLicense_Parsers[i].Flavor);
}

void license_internal::AddFlavor(uint64_t &Value, uint8_t Flavor)
{
    Value |= (((uint64_t)1) << Flavor);
}

const char* license_internal::SupportedFlavor(uint64_t &Value, uint8_t Flavor)
{
    return (Value & (((uint64_t)1) << Flavor)) ? "Yes" : "No ";
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
void license::Feature(feature Value) { license_internal* License = (license_internal*)Internal; License->Input_Flags[0] |= ((uint64_t)1 << Value); }
void license::Muxer(muxer Value) { license_internal* License = (license_internal*)Internal; License->Input_Flags[1] |= ((uint64_t)1 << Value); }
void license::Encoder(encoder Value) { license_internal* License = (license_internal*)Internal; License->Input_Flags[2] |= ((uint64_t)1 << Value); }

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
        if (DecodeLicense(LicenseKey, (license_internal*)Internal))
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
void license::ShowLicense(bool Verbose)
{
    license_internal* License = (license_internal*)Internal;

    // UserID
    if (License->UserID)
    {
        if (Verbose)
        {
            cerr << "License owner ID = " << License->UserID << '.';
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
            *License = license_internal();
        }

        cerr << endl;
    }

    if (!Verbose)
        return;

    // Info
    for (size_t i = 0; Infos[i].Name; i++)
    {
        cerr << "Licensed " << Infos[i].Name << ':' << endl;
        for (int8_t j = 0; j < Infos[i].Size; j++)
        {
            const char* Supported = License->SupportedFlavor(License->License_Flags[i], j);
            cerr << Supported << ' ' << Infos[i].Flavor_String(j) << endl;
        }
        cerr << endl;
    }
}

//---------------------------------------------------------------------------
bool license::IsSupported()
{
    license_internal* License = (license_internal*)Internal;

    // Info
    for (int8_t i = 0; i < 3; i++)
        for (int8_t j = 0; j < Infos[i].Size; j++)
            if (License->Input_Flags[i] & ((uint64_t)1 << j) && License->SupportedFlavor(License->License_Flags[i], j)[0] != 'Y')
                return false;
    
    return true;
}

//---------------------------------------------------------------------------
bool license::IsSupported(feature Feature)
{
    license_internal* License = (license_internal*)Internal;

    const char* Supported = License->SupportedFlavor(License->License_Flags[0], Feature);
    return Supported[0] == 'Y';
}

//---------------------------------------------------------------------------
bool license::IsSupported(parser Parser, uint8_t Style)
{
    license_internal* License = (license_internal*)Internal;

    const char* Supported = License->SupportedFlavor(License->License_Flags[3 + Parser], Style);
    return Supported[0] == 'Y';
}

//---------------------------------------------------------------------------
bool license::IsSupported_License()
{
    string LicenseKey;
    return !DecodeLicense(LicenseKey, NULL);
}
