/*****************************************************************************
 Project:    FarVCS plugin
 Purpose:    Far plugin utilities not specific to FarVCS plugin
*****************************************************************************/

#include <strstream>
#include <algorithm>
#include <windows.h>
#include "plugin.hpp"
#include "farcolor.hpp"
#include "regwrap.h"
#include "plugutil.h"

using namespace std;

/// <summary>
/// Primitive parsing of the contents of an .lng-file into a vector of strings.
/// The contents is read from the '1 FARLANG' resource.
/// </summary>
vector<std::string> ParseLngResource()
{
    vector<std::string> vsMsgs;

    // Get the FARLANG resource

    HRSRC hLangRes = ::FindResource(hResInst, MAKEINTRESOURCE(1), "FARLANG");

    if (hLangRes == 0)
        return vsMsg;

    HGLOBAL hGlobal = ::LoadResource(hResInst, hLangRes);
    if (hGlobal == 0)
        return vsMsg;

    const char *pLangRes = (const char*)::LockResource(hGlobal);
    if (pLangRes == nullptr)
        return vsMsg;

    unsigned long dwSize = ::SizeofResource(hResInst, hLangRes);
    if (dwSize == 0)
        return vsMsg;

    istrstream istr(pLangRes, dwSize);

    char buf[4096];

    while (istr)
    {
        // Get the next line

        istr.getline(buf, sizeof buf);
        if (!istr)
            break;
        buf[_countof(buf) - 1] = '\0';

        // Ignore the line if it doesn't start with quote

        if (buf[0] != '"')
            continue;

        // Get rid of the trailing quote followed by CRLFs

        char *p = buf + strlen(buf) - 1;
        while ( *p == '\n' || *p == '\r' )
            --p;
        if ( *p == '"' )
            --p;
        *(p+1) = '\0';

        // Hanling escape sequences is easier in the std::string

        string s( buf + 1 );

        for ( ; ; )
        {
            size_t i = s.find_first_of( '\\' );
            if ( i == string::npos || i >= s.size()-1 )
                break;
            char c = s[i+1];
            char cReplacement = c == 'r'  ? '\r' :
                                c == 'n'  ? '\n' :
                                c == 'b'  ? '\b' :
                                c == 't'  ? '\t' :
                                c == '\\' ? '\\' :
                                            '\0';
            if ( cReplacement ) {
                s.erase( i, 1 );
                s[i] = cReplacement;
            }
        }

        vsMsgs.push_back( s );
    }

    return vsMsgs;
}

/// <summary>
/// Wrapper around FAR's GetMsg function. Simplifies the call
/// and improves robustness: we survive absence of the lng-file.
/// </summary>
/// <remarks>
/// WARNING! The messages in the lng-file must have contiguous zero-based
/// numbering.
/// </remarks>
const TCHAR *GetMsg(unsigned ing nMsgId)
{
    const TCHAR *szLngMsg = StartupInfo.GetMsg(StartupInfo.ModuleNumber, nMsgId);
    if (*szLngMsg != _T('\0'))
        return szLngMsg;

    // If a message cannot be read from the lng-file, try and extract it
    // from the backup lng file in the resources

    static vector<tstring> vsMsgs;
    static bool bInitialized = false; // Empty vector might also mean unsuccessful parsing, so we have to use a separate flag

    if (!bInitialized)
    {
        vsMsgs = ParseLngResource();
        bInitialized = true;
    }

    return nMsgId < vsMsgs.size() ? vsMsgs[nMsgId].c_str() : szLngMsg;
}

//==========================================================================>>
// Returns the hotkey assigned to the plugin with the given dll name or 0 if
// no hotkeys defined. The dll name must be specified in lower case,
// e.g. "farvcs.dll".
//==========================================================================>>

unsigned char GetPluginHotkey( const char *szDllName )
{
    vector<string> vsHotKeyedPlugins;
    RegWrap( HKEY_CURRENT_USER, "Software\\FAR\\PluginHotkeys" ).EnumKeys( vsHotKeyedPlugins );

    for ( vector<string>::iterator p = vsHotKeyedPlugins.begin(); p != vsHotKeyedPlugins.end(); ++p ) {
        transform( p->begin(), p->end(), p->begin(), tolower );
        if ( p->find( szDllName ) != string::npos ) {
            string sHotkey = RegWrap( HKEY_CURRENT_USER, "Software\\FAR\\PluginHotkeys\\" + *p ).ReadString( "Hotkey" );
            if ( !sHotkey.empty() )
                return (unsigned char)toupper( sHotkey[0] );
        }
    }

    return 0;
}

//==========================================================================>>
// Dialogs preparation
//==========================================================================>>

void InitDialogItems( const InitDialogItem *pInitItems, FarDialogItem *pItems, int nItems )
{
    const InitDialogItem *p = pInitItems;
    FarDialogItem *q = pItems;

    for ( int i = 0; i < nItems; ++i, ++p, ++q )
    {
        q->Type          = p->Type;
        q->X1            = p->X1;
        q->Y1            = p->Y1;
        q->X2            = p->X2;
        q->Y2            = p->Y2;
        q->Focus         = p->Focus;
        q->Selected      = p->Selected;
        q->Flags         = p->Flags;
        q->DefaultButton = p->DefaultButton;

        strncpy( q->Data, p->Data, sizeof q->Data );
        q->Data[sizeof q->Data] = '\0';
    }
}

//==========================================================================>>
// Check if Esc is pressed in the console
//==========================================================================>>

bool CheckForEsc()
{
    HANDLE hConsoleInput = ::GetStdHandle( STD_INPUT_HANDLE );

    if ( !hConsoleInput )
        return false;

    INPUT_RECORD rec;
    DWORD nEventsRead;

    return ::PeekConsoleInput( hConsoleInput, &rec, 1, &nEventsRead ) && nEventsRead > 0 &&
           ::ReadConsoleInput( hConsoleInput, &rec, 1, &nEventsRead ) && nEventsRead > 0 &&
           rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown && rec.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE;
}

//==========================================================================>>
// Run another application from within plugin
//==========================================================================>>

DWORD Execute( const char *szCurDir, const char *szCmdLine, bool bHideOutput, bool bSilent, bool bShowTitle, bool bBackground, const char *szOutputFile )
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    memset( &si, 0, sizeof si );
    si.cb = sizeof si;

    HANDLE hConsoleInput  = ::GetStdHandle( STD_INPUT_HANDLE  );
    HANDLE hConsoleOutput = ::GetStdHandle( STD_OUTPUT_HANDLE );
    HANDLE hConsoleError  = ::GetStdHandle( STD_ERROR_HANDLE  );

    HANDLE hReadPipe = 0;
    HANDLE hWritePipe = 0;
    HANDLE hScreen = 0;

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    memset( &csbi, 0, sizeof csbi ); // Just to get rid of an annoying compiler warning

    WORD outputColor = (WORD)StartupInfo.AdvControl( StartupInfo.ModuleNumber, ACTL_GETCOLOR, (void*)COL_COMMANDLINE );

    if ( bHideOutput )
    {
        SECURITY_ATTRIBUTES sa = { sizeof sa, 0, TRUE };
        const unsigned long cdwPipeBufferSize = 32768;

        if ( ::CreatePipe( &hReadPipe, &hWritePipe, &sa, cdwPipeBufferSize ) )
        {
            ::SetStdHandle( STD_OUTPUT_HANDLE, hWritePipe );
            ::SetStdHandle( STD_ERROR_HANDLE,  hReadPipe );

            if ( !bSilent )
                hScreen = StartupInfo.SaveScreen( 0, 0, -1, -1 );

            /* !!!
            else
                hScreen=Info.SaveScreen(0,0,-1,0);
                Info.Text(2,0,LIGHTGRAY,GetMsg(MWaitForExternalProgram));
            */
        }
        else
            bHideOutput = false;
    }
    else
    {
        // Clear screen

        ::GetConsoleScreenBufferInfo( hConsoleOutput, &csbi );

        char szBlank[nMaxConsoleWidth+1];
        memset( szBlank, ' ', array_size(szBlank) );

        szBlank[min((unsigned int)csbi.dwSize.X,array_size(szBlank)-1)] = 0;

        for ( int i = 0; i < csbi.dwSize.Y; ++i )
            StartupInfo.Text( 0, i, outputColor, szBlank );

        StartupInfo.Text( 0, 0, 0, 0 );

        COORD C = { 0, csbi.dwCursorPosition.Y };
        ::SetConsoleCursorPosition( hConsoleOutput, C );
    }

    DWORD BackupConsoleMode;
    ::GetConsoleMode( hConsoleInput, &BackupConsoleMode );
    ::SetConsoleMode( hConsoleInput, ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_MOUSE_INPUT );

    char szBackupTitle[nMaxConsoleWidth+1];

    if ( bShowTitle )
        ::GetConsoleTitle( szBackupTitle, array_size(szBackupTitle) ),
        SetConsoleTitle( szCmdLine );

    /* $ 14.02.2001 raVen
       делать окошку minimize, если в фоне */
    /* !!! if (Opt.Background)
    {
      si.dwFlags=si.dwFlags | STARTF_USESHOWWINDOW;
      si.wShowWindow=SW_MINIMIZE;
    }
    */
    /* raVen $ */

    DWORD dwExitCode = 0;

    char szVolatileCmdLine[2048]; // ::CreateProcess wants to modify the command line so we create a read-write copy
    array_strcpy( szVolatileCmdLine, szCmdLine );

    BOOL bResult = ::CreateProcess( 0, szVolatileCmdLine, 0, 0, bHideOutput ? TRUE : FALSE,
                                    (bBackground ? CREATE_NEW_CONSOLE : 0), 0, szCurDir, &si, &pi );

    DWORD dwLastError = bResult ? 0 : ::GetLastError();

    if ( bHideOutput )
    {
        ::SetStdHandle( STD_OUTPUT_HANDLE, hConsoleOutput );
        ::SetStdHandle( STD_ERROR_HANDLE, hConsoleError );
        ::CloseHandle( hWritePipe );
    }

    ::SetLastError( dwLastError );

    if ( bResult )
    {
        if ( bHideOutput )
        {
            ::WaitForSingleObject( pi.hProcess, 1000 );

            HANDLE hFile = szOutputFile ? ::CreateFile( szOutputFile, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0 ) : 0;

            char buf[32768];
            DWORD dwRead;
            DWORD dwWritten;
            while ( ::ReadFile( hReadPipe, buf, sizeof buf, &dwRead, 0 ) )
                if ( hFile != INVALID_HANDLE_VALUE )
                    ::WriteFile( hFile, buf, dwRead, &dwWritten, 0 );

            ::CloseHandle( hReadPipe );

            if ( hFile != INVALID_HANDLE_VALUE )
                ::CloseHandle( hFile );
        }
        /* $ 13.09.2000 tran
           фоновой выполнение */
        if ( !bBackground )
        {
            ::WaitForSingleObject( pi.hProcess, INFINITE );
            ::GetExitCodeProcess( pi.hProcess, (LPDWORD)&dwExitCode );
            ::CloseHandle( pi.hThread );
            ::CloseHandle( pi.hProcess );
        }
        else
        {
        /* !!!
            StartThreadForKillListFile(&pi,ListFileName); // нехай за процессом тред следит, и файл бъет тапком
            dwExitCode=0;
        */
        }
        /* tran 13.09.2000 $ */
    }

    if ( bShowTitle )
        ::SetConsoleTitle( szBackupTitle );

    ::SetConsoleMode( hConsoleInput, BackupConsoleMode );

    if ( !bHideOutput )
    {
        SMALL_RECT src = { 0, 2, csbi.dwSize.X, csbi.dwSize.Y };
        COORD dest = { 0, 0 };

        CHAR_INFO fill = { ' ', outputColor };

        ::ScrollConsoleScreenBuffer( hConsoleOutput, &src, 0, dest, &fill );
        StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_SETUSERSCREEN, 0 );
    }

    if ( hScreen )
    {
        StartupInfo.RestoreScreen( 0 );
        StartupInfo.RestoreScreen( hScreen );
    }

    return dwExitCode;
}

HANDLE ExecuteConsoleNoWait( const char *szCurDir, const char *szCmdLine, HANDLE hOutPipe, HANDLE hErrPipe )
{
    HANDLE hConsoleOutput = ::GetStdHandle( STD_OUTPUT_HANDLE );
    HANDLE hConsoleError  = ::GetStdHandle( STD_ERROR_HANDLE  );

    ::SetStdHandle( STD_OUTPUT_HANDLE, hOutPipe );
    ::SetStdHandle( STD_ERROR_HANDLE,  hErrPipe );

    char szVolatileCmdLine[2048]; // ::CreateProcess wants to modify the command line so we create a read-write copy
    array_strcpy( szVolatileCmdLine, szCmdLine );

    STARTUPINFO si;
    PROCESS_INFORMATION pi;

    memset( &si, 0, sizeof si );
    si.cb = sizeof si;

    BOOL bResult = ::CreateProcess( 0, szVolatileCmdLine, 0, 0, TRUE, 0, 0, szCurDir, &si, &pi );

    DWORD dwLastError = bResult ? 0 : ::GetLastError();

    ::SetStdHandle( STD_OUTPUT_HANDLE, hConsoleOutput );
    ::SetStdHandle( STD_ERROR_HANDLE,  hConsoleError );

    ::SetLastError( dwLastError );

    if ( !bResult )
        return INVALID_HANDLE_VALUE;

    ::CloseHandle( pi.hThread );

    return pi.hProcess;
}
