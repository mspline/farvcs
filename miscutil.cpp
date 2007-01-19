/*****************************************************************************
 File name:  miscutil.cpp
 Project:    FarVCS plugin
 Purpose:    General-purpose utilities
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhause
 Dependencies: STL, Win32
*****************************************************************************/

#include "miscutil.h"

using namespace std;

//==========================================================================>>
// Splits a string into a list of strings (like split in perl)
//==========================================================================>>

vector<string> SplitString( const string& s, char c )
{
    vector<string> v;

    // Check if the string is emtpy

    if ( s.empty() )
        return v;

    vector<char> sz( s.length()+1 ); // Used instead of new to utilize auto-deletion
    strcpy( &sz[0], s.c_str() );

    char *pNext = &sz[0];  // The beginning of the next substring

    for ( char *p = &sz[0]; *p != 0; ++p )
    {
        if ( *p == c ) {
            *p = 0;
            v.push_back( pNext );
            if ( c == ' ' ) {
                while ( *(++p) == c )
                    ;
                pNext = p;
            }
            pNext = p + 1;
        }
    }

    if ( *pNext != 0 )
        v.push_back( pNext );

    return v;
}

//==========================================================================>>
// Replacement of the conventional Sleep, because the latter causes troubles
// with message processing -- see MSDN for details
//==========================================================================>>

void SafeSleep( DWORD dwMilliseconds )
{
    static HANDLE hFakeEvent = ::CreateEvent( 0, true /*bManualReset*/, false /*bInitialState*/, 0 );
    HANDLE ObjectsToWait[] = { hFakeEvent };
    ::WaitForMultipleObjects( 1, ObjectsToWait, false, dwMilliseconds );
}

//==========================================================================>>
// Thread class. Encapsulates thread starting and graceful stopping
//==========================================================================>>

HRESULT Thread::Start( void *Param )
{
    if ( m_hThread )   // Already started
        return E_FAIL;

    m_Param = Param;

    // Create the event to signal when we want the thread to terminate

    m_hTerminateEvent = ::CreateEvent( 0, true /*bManualReset*/, false /*bInitialState*/, 0 );
    if ( !m_hTerminateEvent )
        return HRESULT_FROM_WIN32( ::GetLastError() );

    // Spawn the thread

    unsigned int dwDummyThreadID;
    m_hThread = reinterpret_cast<HANDLE>( _beginthreadex( 0, 0, ThreadWrapper, this, 0, &dwDummyThreadID ) );

    return m_hThread ? S_OK : E_FAIL;
}

void Thread::Stop( unsigned long dwMilliseconds )
{
    // Stop the thread if it is running

    if ( m_hThread )
        if ( m_hTerminateEvent == 0 || !::SetEvent( m_hTerminateEvent ) || ::WaitForSingleObject( m_hThread, dwMilliseconds ) != WAIT_OBJECT_0 )
            ::TerminateThread( m_hThread, (DWORD)-1 );

    // Close the temination event and the thread handle

    if ( m_hTerminateEvent ) {
        ::CloseHandle( m_hTerminateEvent );
        m_hTerminateEvent = 0;
    }

    if ( m_hThread ) {
        ::CloseHandle( m_hThread );
        m_hThread = 0;
    }
}

//==========================================================================>>
// GetTempPath/GetTempFileName wrappers
//==========================================================================>>

string GetTempPath()
{
    char szTempPath[MAX_PATH];
    DWORD dwLength = ::GetTempPath( array_size(szTempPath), szTempPath );

    if ( dwLength < array_size(szTempPath) )
        return szTempPath;
    else if ( dwLength == 0 )
        throw std::runtime_error( "::GetTempPath failed" );
    else
        throw std::runtime_error( "::GetTempPath returned a path that was too long" );
}

string GetTempFileName( const char *szPrefix )
{
    char szTempFileName[MAX_PATH];

    if ( ::GetTempFileName( GetTempPath().c_str(), szPrefix ? szPrefix : "", 0, szTempFileName ) == 0 )
        throw std::runtime_error( "::GetTempFileName failed" );

    return szTempFileName;
}
