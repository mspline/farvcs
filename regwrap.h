#pragma once

/*****************************************************************************
 Project:    FarVCS plugin
 Purpose:    Simple wrapper class for registry access
*****************************************************************************/

#include <string>
#include <vector>
#include <windows.h>
#include <tchar.h>
#include "miscutil.h"

#pragma comment(lib,"shlwapi.lib")

/// <summary>
/// A primitive helper class to assist with registry reading/writing
/// </summary>
class RegWrap
{
public:
    RegWrap(HKEY hRootKey, const tstring& sSubKey, REGSAM samDesired = KEY_READ) : hkSession(0) { Init(hRootKey, sSubKey, samDesired); }
    RegWrap(const tstring& sKey, REGSAM samDesired = KEY_READ);
    virtual ~RegWrap();

    RegWrap(const RegWrap&) = delete;
    const RegWrap& operator=(const RegWrap&) = delete;

    bool operator!() const { return hkSession == 0; }

    tstring ReadString(const tstring& sName, LPCTSTR szDefaultValue = 0) const;
    unsigned long ReadDword(const tstring& sName, unsigned long dwDefaultValue = 0) const;

    template <typename T> T Read(const tstring& sName, T defaultValue) const;
    template <> tstring Read<tstring>(const tstring& sName, tstring defaultValue) const { return ReadString(sName, defaultValue.c_str()); }
    template <> unsigned long Read<unsigned long>(const tstring& sName, unsigned long defaultValue) const { return ReadDword(sName, defaultValue); }

    HRESULT WriteString(const tstring& sName, const tstring& sValue) const;
    HRESULT WriteDword(const tstring& sName, unsigned long dwValue) const;

    template <typename T> HRESULT Write(const tstring& sName, T value) const;
    template <> HRESULT Write<tstring>(const tstring& sName, tstring value) const { return WriteString(sName, value); }
    template <> HRESULT Write<unsigned long>(const tstring& sName, unsigned long value) const { return WriteDword(sName, value); }

    HRESULT DeleteValue(const tstring& sName) const;
    HRESULT DeleteKey(const tstring& sName) const;

    HRESULT EnumKeys(std::vector<tstring>& v) const;
    HRESULT EnumValues(std::vector<tstring>& v) const;

    long GetLastError() const { return nInitResult; }

private:
    HKEY hkSession;
    long nInitResult; // The result of the ::RegCreateKeyEx/::RegOpenKeyEx operation

    void Init(HKEY hRootKey, const tstring& sSubKey, REGSAM samDesired);
    long ReadToBuf(const tstring& sName, unsigned long &dwValueType, unsigned char *buf, unsigned long &dwBufSize) const;
    template <typename ENUM_F> HRESULT Enum(std::vector<tstring>& v, ENUM_F enum_f) const;
};
