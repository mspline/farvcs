/*****************************************************************************
 File name:  regwrap.cpp
 Project:    FarCvs plugin
 Purpose:    Simple wrapper class for registry access
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhaus
 Dependencies: STL, Win32
*****************************************************************************/

#include <utility>
#include <shlwapi.h>
#include "regwrap.h"

struct keypair
{
    LPCTSTR szName;
    HKEY hRootKey;
};

static keypair rkam[] =
{
    { _T("HKLM"),                  HKEY_LOCAL_MACHINE },
    { _T("HKCU"),                  HKEY_CURRENT_USER },
    { _T("HKCR"),                  HKEY_CLASSES_ROOT },
    { _T("HKEY_LOCAL_MACHINE"),    HKEY_LOCAL_MACHINE },
    { _T("HKEY_CURRENT_USER"),     HKEY_CURRENT_USER },
    { _T("HKEY_CLASSES_ROOT"),     HKEY_CLASSES_ROOT },
    { _T("HKEY_USERS"),            HKEY_USERS },
    { _T("HKEY_CURRENT_CONFIG"),   HKEY_CURRENT_CONFIG },
    { _T("HKEY_PERFORMANCE_DATA"), HKEY_PERFORMANCE_DATA },
    { _T("HKEY_DYN_DATA"),         HKEY_DYN_DATA },
    { 0, 0 }
};

void RegWrap::Init( HKEY hRootKey, const tstring& sSubKey, REGSAM samDesired )
{
    hkSession = 0;

    // Open/create the registry key

    nInitResult = samDesired & KEY_WRITE ? ::RegCreateKeyEx( hRootKey, sSubKey.c_str(), 0, _T(""), 0, samDesired, 0, &hkSession, 0 )
                                         : ::RegOpenKeyEx(   hRootKey, sSubKey.c_str(), 0, samDesired, &hkSession );

    if ( nInitResult != ERROR_SUCCESS )
        hkSession = 0;
}

RegWrap::RegWrap( const tstring& sKey, REGSAM samDesired ) :
    hkSession( 0 )
{
    // Parse the key

    unsigned long iFirstSlash = sKey.find_first_of( _T("\\") );

    if ( iFirstSlash == tstring::npos )
        return;

    tstring sRootKey = sKey.substr( 0, iFirstSlash );
    tstring sSubKey = sKey.substr( iFirstSlash+1 );

    // Get the root key by its name

    HKEY hRootKey = (HKEY)-1;

    for ( unsigned long i = 0; rkam[i].szName != 0; ++i )
        if ( sRootKey == rkam[i].szName )
            hRootKey = rkam[i].hRootKey;

    if ( hRootKey == (HKEY)-1 )
        return;

    Init( hRootKey, sSubKey, samDesired );
}

RegWrap::~RegWrap()
{
    if ( !this->operator!() )
        ::RegCloseKey( hkSession );
}

tstring RegWrap::ReadString( const tstring& sName, LPCTSTR szDefaultValue ) const
{
    unsigned long dwValueType;
    TCHAR szBuf[4096];
    unsigned long dwBufSize = sizeof szBuf - 1;

    // Read the value into buffer

    if ( ReadToBuf(sName, dwValueType, (LPBYTE)szBuf, dwBufSize) != 0 || dwValueType != REG_SZ )
        return szDefaultValue ? szDefaultValue : _T("");

    return szBuf;
}

unsigned long RegWrap::ReadDword( const tstring& sName, unsigned long dwDefaultValue ) const
{
    unsigned long dwValueType;
    unsigned long dwValue;
    unsigned long dwBufSize = sizeof dwValue;

    // Read the value into buffer

    if ( ReadToBuf(sName, dwValueType, (unsigned char*)&dwValue, dwBufSize) != 0 || dwValueType != REG_DWORD )
        return dwDefaultValue;

    return dwValue;
}

long RegWrap::ReadToBuf( const tstring& sName, unsigned long &dwValueType, unsigned char *buf, unsigned long &dwBufSize ) const
{
    if ( this->operator!() || buf == 0 )
        return -1;

    // Read the value

    long nResult = ::RegQueryValueEx( hkSession, sName.c_str(), 0, &dwValueType, buf, &dwBufSize );

    if ( nResult != ERROR_SUCCESS )
        return -1;

    if ( dwValueType == REG_SZ || dwValueType == REG_MULTI_SZ || dwValueType == REG_EXPAND_SZ )
        buf[dwBufSize] = '\0';

    return 0;
}

HRESULT RegWrap::WriteString( const tstring& sName, const tstring& sValue ) const
{
    if ( this->operator!() )
        return HRESULT_FROM_WIN32( nInitResult );

    long nResult = ::RegSetValueEx( hkSession, sName.c_str(), 0, REG_SZ, (const BYTE *)sValue.c_str(), sValue.length() );
    return HRESULT_FROM_WIN32( nResult );
}

HRESULT RegWrap::WriteDword( const tstring& sName, unsigned long dwValue ) const
{
    if ( this->operator!() )
        return HRESULT_FROM_WIN32( nInitResult );

    long nResult = ::RegSetValueEx( hkSession, sName.c_str(), 0, REG_DWORD, (const BYTE *)&dwValue, sizeof(dwValue) );
    return HRESULT_FROM_WIN32( nResult );
}

HRESULT RegWrap::DeleteValue( const tstring& sName ) const
{
    if ( this->operator!() )
        return HRESULT_FROM_WIN32( nInitResult );

    long nResult = ::RegDeleteValue( hkSession, sName.c_str() );
    return HRESULT_FROM_WIN32( nResult );
}

HRESULT RegWrap::DeleteKey( const tstring& sName ) const
{
    if ( this->operator!() )
        return HRESULT_FROM_WIN32( nInitResult );

    long nResult = ::SHDeleteKey( hkSession, sName.c_str() );
    return HRESULT_FROM_WIN32( nResult );
}

template <typename ENUM_F> HRESULT RegWrap::Enum( std::vector<tstring>& v, ENUM_F enum_f ) const
{
    v.clear();

    if ( this->operator!() )
        return HRESULT_FROM_WIN32( nInitResult );

    long nResult = 0;

    TCHAR szBuf[4096];

    for ( unsigned long i = 0; ; ++i )
    {
        unsigned long dwBufSize = array_size(szBuf);

        nResult = enum_f( hkSession, i, szBuf, &dwBufSize, 0, 0, 0, 0 );

        if ( nResult == ERROR_SUCCESS || nResult == ERROR_MORE_DATA ) {
            v.push_back( szBuf );
            continue;
        }

        if ( nResult == ERROR_NO_MORE_ITEMS ) {
            nResult = S_OK;
            break;
        }
    }
    
    return HRESULT_FROM_WIN32( nResult );
}

HRESULT RegWrap::EnumKeys  ( std::vector<tstring>& v ) const { return Enum( v, ::RegEnumKeyEx ); }
HRESULT RegWrap::EnumValues( std::vector<tstring>& v ) const { return Enum( v, ::RegEnumValue ); }

