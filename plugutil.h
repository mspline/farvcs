#ifndef __PLUGUTIL_H
#define __PLUGUTIL_H

#include <string>
#include <vector>
#include "plugin.hpp"

// These are the backreferences that must be defined in the main plugin module

extern HINSTANCE hInstance;
extern HINSTANCE hResInst;             // Where to load the resources from. May differ from hInstance for second-level plugins
extern PluginStartupInfo StartupInfo;
extern FarStandardFunctions FSF;

extern const char *cszPluginName;

// Utility functions

unsigned char GetPluginHotkey( const char *szDllName );
const char *GetMsg( int nMsgId );

// Dialogs preparation

struct InitDialogItem
{
    unsigned char Type;
    unsigned char X1,Y1,X2,Y2;
    unsigned char Focus;
    unsigned int Selected;
    unsigned int Flags;
    unsigned char DefaultButton;
    char *Data;
};

void InitDialogItems( const InitDialogItem *pInitItems, FarDialogItem *pItems, int nItems );

// Check if Esc is pressed in the console

bool CheckForEsc();
        
// Execute an external console application

DWORD Execute( const char *szCurDir, const char *szCmdLine, bool bHideOutput, bool bSilent, bool bShowTitle, bool bBackground, const char *szOutputFile = 0 );
HANDLE ExecuteConsoleNoWait( const char *szCurDir, const char *szCmdLine, HANDLE hOutPipe, HANDLE hErrPipe );

const unsigned int nMaxConsoleWidth  = 300;
const unsigned int nMaxConsoleHeight = 150;

//==========================================================================>>
// Wrapper around the StartupInfo::Message function
//==========================================================================>>

inline void MsgBoxWarning( const char *szTitle, const char *szFormat, ... )
{
    va_list args;
    char szBuf[4096];

    array_strcpy( szBuf, szTitle );
    if ( strlen(szBuf) < sizeof szBuf - 2 )
        strcat( szBuf, "\n" );

    va_start( args, szFormat );
    _vsnprintf( szBuf+strlen(szBuf), array_size(szBuf)-strlen(szBuf)-1, szFormat, args );
    szBuf[sizeof szBuf-1] = 0;

    StartupInfo.Message( StartupInfo.ModuleNumber,
                         FMSG_WARNING | FMSG_ALLINONE | FMSG_MB_OK,
                         0,   // Help topic
                         (const char * const *)szBuf,
                         0,   // Items number
                         0 ); // Buttons number
}

#endif // __PLUGUTIL_H
