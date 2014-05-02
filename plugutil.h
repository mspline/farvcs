#pragma once

#include <stdio.h>
#include <tchar.h>
#include <string>
#include <vector>
#include "winhelpers.h"
#include "farsdk/plugin.hpp"

extern HINSTANCE hInstance;
extern HINSTANCE hResInst;             // Where to load the resources from. May differ from hInstance for second-level plugins.
extern PluginStartupInfo StartupInfo;
extern FarStandardFunctions FSF;

const extern TCHAR *cszPluginName;
const extern GUID PluginGuid;

// Utility functions

unsigned char GetPluginHotkey(const TCHAR *szDllName);
const TCHAR *GetMsg(int nMsgId);

// Dialogs preparation

struct InitDialogItem
{
    unsigned char Type;
    unsigned int X1,Y1,X2,Y2;
    unsigned char Focus;
    unsigned int Selected;
    unsigned int Flags;
    unsigned char DefaultButton;
    char *Data;
};

void InitDialogItems(const InitDialogItem *pInitItems, FarDialogItem *pItems, size_t nItems);

// Checks if Esc is pressed in the console

bool CheckForEsc();
        
// Executes an external console application

DWORD Execute(const TCHAR *szCurDir, const TCHAR *szCmdLine, bool bHideOutput, bool bSilent, bool bShowTitle, bool bBackground, const char *szOutputFile = 0);
HANDLE ExecuteConsoleNoWait(const TCHAR *szCurDir, const TCHAR *szCmdLine, HANDLE hOutPipe, HANDLE hErrPipe);

const unsigned int nMaxConsoleWidth  = 1000; // Should be enough for the smallest font on 4K screen
const unsigned int nMaxConsoleHeight = 500;

/// <summary>
/// Wrapper around the <c>StartupInfo::Message</c> function.
/// </summary>
inline void MsgBoxWarning(const TCHAR *szTitle, const TCHAR *szFormat, ...)
{
    va_list args;
    TCHAR szBuf[2048];

    _tcsncpy_s(szBuf, szTitle, _TRUNCATE);
    _tcsncat_s(szBuf, _T("\n"), _TRUNCATE);

    va_start(args, szFormat);
    _vsntprintf_s(szBuf + _tcslen(szBuf), _countof(szBuf) - _tcslen(szBuf), _TRUNCATE, szFormat, args);

    StartupInfo.Message(&PluginGuid,
                        &PluginGuid,                                    // !!! Must be replaced with unique message id
                        FMSG_WARNING | FMSG_ALLINONE | FMSG_MB_OK,
                        0,                                              // Help topic - a dummy value
                        reinterpret_cast<const TCHAR * const *>(szBuf), // Items - Cast required because of FMSG_ALLINONE
                        0,                                              // ItemsNumber - ignored because of FMSG_ALLINONE
                        0);                                             // ButtonsNumber - ignored because of FMSG_MB_*
}

/// <summary>
/// Get the size of the Far's console window.
/// </summary>
/// <returns>Console size if succeessful; otherwise, <c>{0, 0}</c>.</returns>
inline COORD GetConsoleSize()
{
    COORD coord = { 0, 0 };

    HANDLE hConsoleOutput = ::GetStdHandle(STD_OUTPUT_HANDLE);

    if (!hConsoleOutput)
        return coord;

    CONSOLE_SCREEN_BUFFER_INFO screenBufInfo;
    if (!::GetConsoleScreenBufferInfo( hConsoleOutput, &screenBufInfo))
        return coord;

    coord.X = screenBufInfo.dwSize.X;
    coord.Y = screenBufInfo.dwSize.Y;

    return coord;
}
