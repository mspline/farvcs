#ifndef __LONGOP_H
#define __LONGOP_H

/*****************************************************************************
 File name:  longop.h
 Project:    FarVCS plugin
 Purpose:    Wrappers for long operations in FAR plugins
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhause
 Dependencies: STL
*****************************************************************************/

#include <string>
#include <vector>
#include <memory.h>
#include <windows.h>
#include "farcolor.hpp"
#include "enforce.h"
#include "plugin.hpp"
#include "miscutil.h"
#include "plugutil.h"
#include "lang.h"

class LongOperation
{
public:
    LongOperation( const char *szPluginName ) : m_szPluginName( szPluginName ), m_hDlg( 0 ) {}
    virtual ~LongOperation() {}

    bool Execute()
    {
        std::vector<InitDialogItem> InitItems;
        RECT r;

        DoGetDlg( r, InitItems );
        if ( InitItems.empty() )
            return false;

        std::vector<FarDialogItem> DialogItems( InitItems.size()+1 ); // Far crashes when using just InitItems.size()
        InitDialogItems( &InitItems[0], &DialogItems[0], InitItems.size() );

        return StartupInfo.DialogEx( StartupInfo.ModuleNumber, r.left, r.top, r.right, r.bottom, 0, &DialogItems[0], DialogItems.size(), 0, 0, DlgProc, reinterpret_cast<long>( this ) ) != 0;
    }

protected:
    const char *GetPluginName() { return m_szPluginName; }
    HANDLE GetDlg() { return m_hDlg; }

    virtual void DoGetDlg( RECT& r, std::vector<InitDialogItem>& vInitItems ) = 0;
    virtual bool DoExecute() = 0;
    virtual void DrawDlgItem( int /*id*/, FarDialogItem * /*pitem*/ ) {}

    const char *m_szPluginName;
    HANDLE m_hDlg;

private:
    static long WINAPI DlgProc( HANDLE hDlg, int Msg, int Param1, long Param2 )
    {
        static LongOperation *pThis;

        if ( Msg == DN_INITDIALOG )
        {
            pThis = reinterpret_cast<LongOperation *>( Param2 );
            pThis->m_hDlg = hDlg;
        }
        else if ( Msg == DN_ENTERIDLE )
        {
            bool bResult = false;
            
            try
            {
                bResult = pThis->DoExecute();
            }
            catch( std::runtime_error& e )
            {
                MsgBoxWarning( pThis->m_szPluginName, e.what() );
            }

            StartupInfo.SendDlgMessage( hDlg, DM_CLOSE, bResult ? 1 : 0, 0 );
        }
        else if ( Msg == DN_DRAWDLGITEM )
        {
            pThis->DrawDlgItem( (int)Param1, (FarDialogItem*)Param2 );
        }

        return StartupInfo.DefDlgProc( hDlg, Msg, Param1, Param2 );
    }
};

class SimpleLongOperation : public LongOperation
{
public:
    SimpleLongOperation( const char *szPluginName ) : LongOperation( szPluginName ), m_dwLastUserInteraction(0) {}

protected:
    virtual void DoGetDlg( RECT& r, std::vector<InitDialogItem>& vInitItems )
    {
        static char szProgressBg[nMaxConsoleWidth];
        ::memset( szProgressBg, 0xB0, W() );

        unsigned long dwPromptId = DoGetPrompt();
        const char *szPrompt = (dwPromptId > 1000) ? reinterpret_cast<const char*>(dwPromptId) : GetMsg(dwPromptId);

        InitDialogItem InitItems[] =
        {
            /* 0 */ { DI_DOUBLEBOX, 3,1,W()+6,5,  0,0, 0, 0, const_cast<char*>( GetPluginName() )    },
            /* 1 */ { DI_TEXT,      5,2,0,0,      0,0, 0, 0, const_cast<char*>( szPrompt )           },
            /* 2 */ { DI_TEXT,      5,3,0,0,      0,0, 0, 0, const_cast<char*>( DoGetInitialInfo() ) },
            /* 3 */ { DI_TEXT,      5,4,0,0,      0,0, 0, 0, const_cast<char*>( szProgressBg )       }
        };

        vInitItems.clear();
        vInitItems.assign( InitItems, InitItems+array_size(InitItems) );

        r.left = -1;
        r.top = -1;
        r.right = W()+10;
        r.bottom = 7;
    }

    bool Interaction( const char *szCurrentPath, int percentage, bool bForce = false )
    {
        // Updating UI and processing the user input are time-consuming
        // operations. We don't want to to that too often

        if ( !bForce && ::GetTickCount() - m_dwLastUserInteraction < 100 )
            return false;

        m_dwLastUserInteraction = ::GetTickCount();
            
        static char szBackground[nMaxConsoleWidth];
        ::memset( szBackground, ' ', sizeof szBackground );

        static char szProgress[nMaxConsoleWidth];
        ::memset( szProgress, 0xDB, sizeof szProgress );

        szProgress[W()*min(max(percentage,0),100)/100] = 0;

        char szText[MAX_PATH];
        array_strcpy( szText, szCurrentPath );
        FSF.TruncPathStr( szText, W() );

        FarDialogItemData background = { W(), szBackground };
        FarDialogItemData text       = { strlen(szText), szText };
        FarDialogItemData progress   = { strlen(szProgress), szProgress };

        StartupInfo.SendDlgMessage( GetDlg(), DM_SETTEXT, 2, (long)&background );
        StartupInfo.SendDlgMessage( GetDlg(), DM_SETTEXT, 2, (long)&text );
        StartupInfo.SendDlgMessage( GetDlg(), DM_SETTEXT, 3, (long)&progress );

        return CheckForEsc();
    }

    unsigned int W() const { return 66; } // Status line width

    virtual unsigned long DoGetPrompt() = 0;
    virtual const char *DoGetInitialInfo() = 0;

private:
    DWORD m_dwLastUserInteraction;  // Tracks the time of the last user interaction
};

class ScrollLongOperation : public LongOperation
{
public:
    ScrollLongOperation( const char *szPluginName ) : LongOperation( szPluginName ), m_dwLastUserInteraction(0) {}

protected:
    virtual void DoGetDlg( RECT& r, std::vector<InitDialogItem>& vInitItems )
    {
        static std::vector<CHAR_INFO> VBuf( nMaxConsoleWidth * nMaxConsoleHeight ); // Allocate maximum. Who cares about extra 30kB?

        CHAR_INFO ciInit = { ' ', (WORD)StartupInfo.AdvControl( StartupInfo.ModuleNumber, ACTL_GETCOLOR, (void*)COL_DIALOGBOX ) };
        
        for ( unsigned int i = 0; i < VBuf.size(); ++i )
            VBuf[i] = ciInit;

        unsigned long dwPromptId = DoGetPrompt();
        const char *szPrompt = (dwPromptId > 1000) ? reinterpret_cast<const char*>(dwPromptId) : GetMsg(dwPromptId);

        InitDialogItem InitItems[] =
        {
            /* 0 */ { DI_DOUBLEBOX,   3,   1, W()+6, H()+4, 0,0,                      0, 0, const_cast<char*>( GetPluginName() ) },
            /* 1 */ { DI_TEXT,        5,   2, 0,     0,     0,0,                      0, 0, const_cast<char*>( szPrompt )        },
            /* 2 */ { DI_TEXT,        5,   3, 0,     0,     0,0,                      DIF_BOXCOLOR | DIF_SEPARATOR, 0, ""        },
            /* 3 */ { DI_USERCONTROL, 5,   4, W()+4, H()+3, 0,(unsigned int)&VBuf[0], 0, 0, ""                                   }
        };

        vInitItems.clear();
        vInitItems.assign( InitItems, InitItems+array_size(InitItems) );

        r.left = -1;
        r.top = -1;
        r.right = W()+10;
        r.bottom = H()+6;
    }

    void Scroll( const char *szNextLine )
    {
        if ( vLines.size() >= H() )
            vLines.erase( vLines.begin() );

        if ( !vLines.empty() || *szNextLine )
            vLines.push_back( ProkrustString(szNextLine,W()) );
    }

    bool Interaction( bool bForce = true )
    {
        // Updating UI and processing the user input are time-consuming
        // operations. We don't want it to happen too often

        if ( !bForce && ::GetTickCount() - m_dwLastUserInteraction < 100 )
            return false;

        m_dwLastUserInteraction = ::GetTickCount();
            
        StartupInfo.SendDlgMessage( GetDlg(), DM_SHOWITEM, 3, 1 );
        return CheckForEsc();
    }

    void DrawDlgItem( int id, FarDialogItem *pitem )
    {
        if ( id != 3 )
            return;

        CHAR_INFO *VBuf = pitem->VBuf;

        for ( unsigned int i = 0; i < vLines.size(); ++i )
            for ( unsigned int j = 0; j < W(); ++j )
                VBuf[i*W()+j].Char.AsciiChar = vLines[i][j];
    }

    virtual unsigned long DoGetPrompt() = 0;

protected:
    unsigned int W() const { static unsigned int W = GetConsoleSize().X*4/5; return W; } // Scroll control width
    unsigned int H() const { static unsigned int H = GetConsoleSize().Y*2/3; return H; } // Scroll control height

private:
    DWORD m_dwLastUserInteraction;  // Tracks the time of the last user interaction
    std::vector<std::string> vLines;
};

class Executor : public ScrollLongOperation
{
public:
    Executor( const char *szPluginName, const char *szDir, const std::string& sCmdLine, const boost::function<void(char*)>* const pf = 0, const char *szTmpFile = 0 ) :
        ScrollLongOperation( szPluginName ),
        m_szDir( szDir ),
        m_sCmdLine( sCmdLine ),
        m_pfNewLineCallback( pf ),
        m_sTempFileName( szTmpFile ? szTmpFile : "" ),
        m_sPrompt( ProkrustString( sCmdLine, W() ) )
    {}

protected:
    virtual unsigned long DoGetPrompt() { return reinterpret_cast<unsigned long>( m_sPrompt.c_str() ); }

    virtual bool DoExecute()
    {
        std::string sPipeName = GetTempPipeName();

        SECURITY_ATTRIBUTES sa = { sizeof sa, 0, TRUE };
        const unsigned long cdwPipeBufferSize = 1024;

        // We have to use named pipe solely because unnamed pipes don't support
        // asynchronous operations

        W32Handle HReadPipe = ENF_H( ::CreateNamedPipe( sPipeName.c_str(),
                                                        PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                                                        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                                                        1,
                                                        cdwPipeBufferSize,
                                                        cdwPipeBufferSize,
                                                        500,
                                                        0 ) );

        W32Handle HWritePipe = ENF_H( ::CreateFile( sPipeName.c_str(), GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, 0 ) );

        W32Handle HProcess = ExecuteConsoleNoWait( m_szDir,
                                                   m_sCmdLine.c_str(),
                                                   HWritePipe,
                                                   m_sTempFileName.empty() ? HWritePipe : 0 );
        HWritePipe.Close();

        if ( !HProcess )
        {
            MsgBoxWarning( GetPluginName(), "Cannot execute external application:\n%s\n%s", m_sCmdLine.c_str(), LastErrorStr().c_str() );
            return false;
        }

        W32Handle HTempFile = !m_sTempFileName.empty() ? ENF_H( ::CreateFile( m_sTempFileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0 ) )
                                                       : INVALID_HANDLE_VALUE;

        if ( !HTempFile && !m_sTempFileName.empty() )
        {
            MsgBoxWarning( GetPluginName(), "Cannot create temporary file:\n%s\n%s", m_sTempFileName.c_str(), LastErrorStr().c_str() );
            return false;
        }

        char buf[cdwPipeBufferSize];
        DWORD dwRead = 0;
        DWORD dwWritten;

        W32Event oe;
        OVERLAPPED o = { 0, 0, 0, 0, oe };

        unsigned long nr = 0;       // Number of lines read
        bool bIoPending = false;    // Overlapped IO is in progress
        char szLine[4096] = "";     // Current line is collected here

        for ( ; ; )
        {
            if ( !bIoPending && !::ReadFile( HReadPipe, buf, sizeof buf, &dwRead, &o ) )
            {
                if ( ::GetLastError() == ERROR_BROKEN_PIPE )
                    break;
                else if ( ENF( ::GetLastError() == ERROR_IO_PENDING ) )
                    bIoPending = true;
            }

            if ( bIoPending && ::WaitForSingleObjectEx( oe, 1000, TRUE ) == WAIT_OBJECT_0 )
            {
                bIoPending = false;

                if ( !::GetOverlappedResult( HReadPipe, &o, &dwRead, FALSE ) && ENF( ::GetLastError() == ERROR_BROKEN_PIPE ) )
                    break;
            }

            if ( Interaction() )
            {
                ::TerminateProcess( HProcess, (UINT)-1 );
                return false;
            }

            if ( bIoPending )
                continue;

            if ( HTempFile.IsValid() )
                ENF( ::WriteFile( HTempFile, buf, dwRead, &dwWritten, 0 ) );

            if ( dwRead > 0 )
            {
                for ( char *p = buf; ; )
                {
                    char *pNextEOL = std::find( p, buf+dwRead, '\n' );
                    
                    if ( pNextEOL > buf && pNextEOL < buf+dwRead && pNextEOL[-1] == '\r' )
                        pNextEOL[-1] = 0;

                    array_strncat( szLine, p, pNextEOL-p );

                    if ( pNextEOL == buf+dwRead )
                        break;

                    if ( m_pfNewLineCallback )
                        (*m_pfNewLineCallback)( szLine );

                    Scroll( szLine );
                    *szLine = 0;

                    p = pNextEOL+1;
                }
            }

            nr += std::count( buf, buf+dwRead, '\n' );
        }

        if ( *szLine != 0 )
        {
            (*m_pfNewLineCallback)( szLine );
            Scroll( szLine );
            *szLine = 0;
        }

        Interaction( true );  // So that the dialog blinks with the latest data before dying
        SleepEx( 100, TRUE ); // To increase the probability of latest data appearing onscreen

        ENF( ::WaitForSingleObjectEx( HProcess, INFINITE, TRUE ) == WAIT_OBJECT_0 );
        
        DWORD dwExitCode;
        ENF( ::GetExitCodeProcess( HProcess, (LPDWORD)&dwExitCode ) );

        return dwExitCode == 0;
    }

    const char *m_szDir;
    std::string m_sCmdLine;
    std::string m_sTempFileName;
    const boost::function<void(char*)>* m_pfNewLineCallback;
    std::string m_sPrompt;
};

#endif // __LONGOP_H
