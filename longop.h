#pragma once

/*****************************************************************************
 Project:    FarVCS plugin
 Purpose:    Wrappers for long operations in FAR plugins
*****************************************************************************/

#include <string>
#include <vector>
#include <memory.h>
#include <windows.h>
#include "farsdk/plugin.hpp"
#include "farsdk/farcolor.hpp"
#include "enforce.h"
#include "miscutil.h"
#include "plugutil.h"
#include "lang.h"

class LongOperation
{
public:
    LongOperation(const TCHAR *szPluginName) : m_pluginName(szPluginName), m_hDlg(0) {}
    virtual ~LongOperation() {}

    intptr_t Execute()
    {
        m_hDlg = DoGetDlg();

        if (m_hDlg == INVALID_HANDLE_VALUE)
            return 0;

        intptr_t result = StartupInfo.DialogRun(m_hDlg);
        StartupInfo.DialogFree(m_hDlg);
        return result;
    }

protected:
    const TCHAR *GetPluginName() { return m_pluginName.c_str(); }

    virtual HANDLE DoGetDlg() = 0;
    virtual bool DoExecute() = 0;
    virtual void DrawDlgItem(int /*id*/, FarDialogItem * /*pitem*/) {}

    tstring m_pluginName;
    HANDLE m_hDlg;

    /// <summary>
    /// Dialog procedure intended to be shared by all classes implementing <c>LongOperation</c>.
    /// </summary>
    /// <remarks>
    /// Only one <c>LongOperation</c> dialog at at ime can be active.
    /// </remarks>
    static intptr_t WINAPI DlgProc(HANDLE hDlg, intptr_t Msg, intptr_t Param1, void *Param2)
    {
        static LongOperation *pThis; // static -- this is why only one dialog at a time.

        if (Msg == DN_INITDIALOG)
        {
            pThis = reinterpret_cast<LongOperation *>(Param2);
            pThis->m_hDlg = hDlg;
        }
        else if (Msg == DN_ENTERIDLE)
        {
            bool bResult = false;
            
            try
            {
                bResult = pThis->DoExecute();
            }
            catch(std::runtime_error& e)
            {
                MsgBoxWarning(pThis->m_pluginName.c_str(), _T("%s"), e.what());
            }

            StartupInfo.SendDlgMessage(hDlg, DM_CLOSE, bResult ? 1 : 0, 0);
        }
        else if (Msg == DN_DRAWDLGITEM)
        {
            pThis->DrawDlgItem((int)Param1, (FarDialogItem*)Param2);
        }

        return StartupInfo.DefDlgProc(hDlg, Msg, Param1, Param2);
    }
};

class SimpleLongOperation : public LongOperation
{
public:
    SimpleLongOperation(const TCHAR *szPluginName) : LongOperation(szPluginName), m_dwLastUserInteraction(0) {}

protected:
    virtual HANDLE DoGetDlg() override
    {
        static TCHAR szProgressBg[nMaxConsoleWidth];
        std::uninitialized_fill_n(szProgressBg, 0xB0, W());
        szProgressBg[_countof(szProgressBg) - 1] = 0;

        intptr_t dwPromptId = DoGetPrompt();
        const TCHAR *szPrompt = dwPromptId > 1000 ? reinterpret_cast<const TCHAR*>(dwPromptId) : GetMsg(static_cast<int>(dwPromptId));

        FarDialogItem DlgItems[] =
        {
            /* 0 */ { DI_DOUBLEBOX, 3,1,W()+6,5,  0, nullptr,nullptr, 0, GetPluginName(),    0,0, { 0, 0 } },
            /* 1 */ { DI_TEXT,      5,2,0,0,      0, nullptr,nullptr, 0, szPrompt,           0,0, { 0, 0 } },
            /* 2 */ { DI_TEXT,      5,3,0,0,      0, nullptr,nullptr, 0, DoGetInitialText(), 0,0, { 0, 0 } },
            /* 3 */ { DI_TEXT,      5,4,0,0,      0, nullptr,nullptr, 0, szProgressBg,       0,0, { 0, 0 } }
        };

        StartupInfo.DialogInit(
            &PluginGuid,
            &PluginGuid, // !!! Replace with proper dialog guid
            -1, -1, W() + 10, 7,
            0, // HelpTopic
            DlgItems,
            _countof(DlgItems),
            0, // Reserved
            FDLG_NONE,
            DlgProc,
            this);
    }

    bool UserInteraction(const TCHAR *szCurrentPath, unsigned short progressPercent, bool bForce = false)
    {
        // Updating UI and processing the user input are time-consuming
        // operations. We don't want to do that too often.

        if ( !bForce && ::GetTickCount() - m_dwLastUserInteraction < 100 )
            return false;

        m_dwLastUserInteraction = ::GetTickCount();
            
        static TCHAR szBackground[nMaxConsoleWidth];
        std::uninitialized_fill_n(szBackground, _countof(szBackground), _T(' '));
        szBackground[_countof(szBackground) - 1] = 0;

        static TCHAR szProgress[nMaxConsoleWidth];
        std::uninitialized_fill_n(szProgress, _countof(szProgress), 0xDB);
        szProgress[_countof(szProgress) - 1] = 0;

        szProgress[W() * min(progressPercent, 100) / 100] = 0;

        TCHAR szText[MAX_PATH];
        _tcsncpy_s(szText, szCurrentPath, _TRUNCATE);
        FSF.TruncPathStr(szText, W());

        FarDialogItemData background = { sizeof(FarDialogItemData), _tcslen(szBackground), szBackground };
        FarDialogItemData text       = { sizeof(FarDialogItemData), _tcslen(szText),       szText };
        FarDialogItemData progress   = { sizeof(FarDialogItemData), _tcslen(szProgress),   szProgress };

        StartupInfo.SendDlgMessage(m_hDlg, DM_SETTEXT, 2, &background);
        StartupInfo.SendDlgMessage(m_hDlg, DM_SETTEXT, 2, &text);
        StartupInfo.SendDlgMessage(m_hDlg, DM_SETTEXT, 3, &progress);

        return CheckForEsc();
    }

    unsigned int W() const { return 66; } // Status line width

    virtual intptr_t DoGetPrompt() = 0;
    virtual const TCHAR *DoGetInitialText() = 0;

private:
    DWORD m_dwLastUserInteraction;
};

class ScrollLongOperation : public LongOperation
{
public:
    ScrollLongOperation(const TCHAR *szPluginName) : LongOperation(szPluginName), m_dwLastUserInteraction(0) {}

protected:
    virtual HANDLE DoGetDlg() override
    {
        static std::vector<FAR_CHAR_INFO> VBuf;
        VBuf.reserve(nMaxConsoleWidth * nMaxConsoleHeight); // Allocate maximum. Who cares about an extra 1MB?

        FarColor dlgColor;
        if (!StartupInfo.AdvControl(&PluginGuid, ACTL_GETCOLOR, COL_DIALOGBOX, &dlgColor))
            throw std::runtime_error("ACTL_GETCOLOR failed.");

        std::uninitialized_fill_n(VBuf.begin(), VBuf.size(), FAR_CHAR_INFO{ _T(' '), dlgColor });

        intptr_t dwPromptId = DoGetPrompt();
        const TCHAR *szPrompt = dwPromptId > 1000 ? reinterpret_cast<const TCHAR*>(dwPromptId) : GetMsg(static_cast<int>(dwPromptId));

        FarDialogItem DlgItems[] =
        {
            /* 0 */ { DI_DOUBLEBOX,   3,1,W()+6,H()+4, 0,                                    nullptr,nullptr, 0,                            GetPluginName(),  0,0, { 0, 0 } },
            /* 1 */ { DI_TEXT,        5,2,0,0,         0,                                    nullptr,nullptr, 0,                            szPrompt,         0,0, { 0, 0 } },
            /* 2 */ { DI_TEXT,        5,3,0,0,         0,                                    nullptr,nullptr, DIF_BOXCOLOR | DIF_SEPARATOR, _T(""),           0,0, { 0, 0 } },
            /* 3 */ { DI_USERCONTROL, 5,4,W()+4,H()+3, reinterpret_cast<intptr_t>(&VBuf[0]), nullptr,nullptr, 0,                            nullptr,          0,0, { 0, 0 } }
        };

        StartupInfo.DialogInit(
            &PluginGuid,
            &PluginGuid, // !!! Replace with proper dialog guid
            -1, -1, W() + 10, H() + 6,
            0, // HelpTopic
            DlgItems,
            _countof(DlgItems),
            0, // Reserved
            FDLG_NONE,
            DlgProc,
            this);
    }

    void Scroll(const TCHAR *szNextLine)
    {
        if (vLines.size() >= H())
            vLines.erase(vLines.begin());

        if (!vLines.empty() || *szNextLine)
            vLines.push_back(ProkrustString(szNextLine, W()));
    }

    bool UserInteraction(bool bForce = true)
    {
        // Updating UI and processing the user input are time-consuming
        // operations. We don't want it to happen too often.

        if (!bForce && ::GetTickCount() - m_dwLastUserInteraction < 100)
            return false;

        m_dwLastUserInteraction = ::GetTickCount();
            
        StartupInfo.SendDlgMessage(m_hDlg, DM_SHOWITEM, 3, reinterpret_cast<void*>(1) );
        return CheckForEsc();
    }

    void DrawDlgItem(int id, FarDialogItem *pitem) override
    {
        if (id != 3)
            return;

        FAR_CHAR_INFO *vBuf = pitem->VBuf;

        for (size_t i = 0; i < vLines.size(); ++i)
            for (size_t j = 0; j < W(); ++j )
                vBuf[i * W() + j].Char = vLines[i][j];
    }

    virtual intptr_t DoGetPrompt() = 0;

protected:
    unsigned int W() const { static unsigned int W = GetConsoleSize().X*4/5; return W; } // Scroll control width. TODO: Handle dynamically changing console size.
    unsigned int H() const { static unsigned int H = GetConsoleSize().Y*2/3; return H; } // Scroll control height. TODO: Handle dynamically changing console size.

private:
    DWORD m_dwLastUserInteraction;
    std::vector<tstring> vLines;
};

class Executor : public ScrollLongOperation
{
public:
    Executor(const TCHAR *szPluginName, const TCHAR *szDir, const tstring& sCmdLine, const boost::function<void(TCHAR*)>& fNewLineCallback, const TCHAR *szTmpFile = 0) :
        ScrollLongOperation(szPluginName),
        m_szDir(szDir),
        m_sCmdLine(sCmdLine),
        m_fNewLineCallback(fNewLineCallback),
        m_sTempFileName(szTmpFile ? szTmpFile : _T("")),
        m_sPrompt(ProkrustString(sCmdLine, W()))
    {}

protected:
    virtual intptr_t DoGetPrompt() override { return reinterpret_cast<intptr_t>(m_sPrompt.c_str()); }

    virtual bool DoExecute() override
    {
        tstring sPipeName = GetTempPipeName();

        SECURITY_ATTRIBUTES sa = { sizeof sa, 0, TRUE };
        const unsigned long cdwPipeBufferSize = 1024;

        // We have to use named pipe solely because unnamed pipes don't support
        // asynchronous operations

        W32Handle HReadPipe = ENF_H(::CreateNamedPipe(sPipeName.c_str(),
                                                      PIPE_ACCESS_INBOUND | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                                                      PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                                                      1,
                                                      cdwPipeBufferSize,
                                                      cdwPipeBufferSize,
                                                      500,
                                                      0));

        W32Handle HWritePipe = ENF_H(::CreateFile(sPipeName.c_str(), GENERIC_WRITE, 0, &sa, OPEN_EXISTING, 0, 0));

        W32Handle HProcess = ExecuteConsoleNoWait(m_szDir,
                                                  m_sCmdLine.c_str(),
                                                  HWritePipe,
                                                  m_sTempFileName.empty() ? HWritePipe : 0);
        HWritePipe.Close();

        if (!HProcess)
        {
            MsgBoxWarning(GetPluginName(), _T("Cannot execute external application:\n%s\n%s"), m_sCmdLine.c_str(), LastErrorStr().c_str());
            return false;
        }

        W32Handle HTempFile = !m_sTempFileName.empty() ? ENF_H(::CreateFile(m_sTempFileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0))
                                                       : INVALID_HANDLE_VALUE;

        if (!HTempFile && !m_sTempFileName.empty())
        {
            MsgBoxWarning(GetPluginName(), _T("Cannot create temporary file:\n%s\n%s"), m_sTempFileName.c_str(), LastErrorStr().c_str());
            return false;
        }

        TCHAR buf[cdwPipeBufferSize];
        DWORD dwRead = 0;
        DWORD dwWritten;

        W32Event oe;
        OVERLAPPED o = { 0, 0, 0, 0, oe };

        unsigned long nr = 0;         // Number of lines read
        bool bIoPending = false;      // Overlapped IO is in progress
        TCHAR szLine[4096] = _T("");  // Current line is collected here

        for ( ; ; )
        {
            if (!bIoPending && !::ReadFile(HReadPipe, buf, sizeof buf, &dwRead, &o))
            {
                if (::GetLastError() == ERROR_BROKEN_PIPE)
                    break;
                else if (ENF(::GetLastError() == ERROR_IO_PENDING))
                    bIoPending = true;
            }

            if (bIoPending && ::WaitForSingleObjectEx(oe, 1000, TRUE) == WAIT_OBJECT_0)
            {
                bIoPending = false;

                if (!::GetOverlappedResult(HReadPipe, &o, &dwRead, FALSE) && ENF(::GetLastError() == ERROR_BROKEN_PIPE))
                    break;
            }

            if (UserInteraction())
            {
                ::TerminateProcess(HProcess, (UINT)-1);
                return false;
            }

            if (bIoPending)
                continue;

            if (HTempFile.IsValid())
                ENF(::WriteFile(HTempFile, buf, dwRead, &dwWritten, 0));

            if (dwRead > 0)
            {
                for (TCHAR *p = buf; ;)
                {
                    TCHAR *pNextEOL = std::find( p, buf + dwRead, '\n' );
                    
                    if (pNextEOL > buf && pNextEOL < buf + dwRead && pNextEOL[-1] == _T('\r'))
                        pNextEOL[-1] = _T('\0');

                    _tcsncat_s(szLine, p, _TRUNCATE);

                    if (pNextEOL == buf + dwRead)
                        break;

                    m_fNewLineCallback(szLine);

                    Scroll(szLine);
                    *szLine = _T('\0');

                    p = pNextEOL + 1;
                }
            }

            nr += static_cast<unsigned long>(std::count(buf, buf + dwRead, _T('\n')));
        }

        if (*szLine != 0)
        {
            m_fNewLineCallback(szLine);
            Scroll(szLine);
            *szLine = _T('\0');
        }

        UserInteraction(true);  // So that the dialog displays the latest data before closing
        SleepEx(100, TRUE);     // To increase the probability of latest data appearing onscreen

        ENF(::WaitForSingleObjectEx(HProcess, INFINITE, TRUE) == WAIT_OBJECT_0);
        
        DWORD dwExitCode;
        ENF(::GetExitCodeProcess(HProcess, (LPDWORD)&dwExitCode));

        return dwExitCode == 0;
    }

    const TCHAR *m_szDir;
    tstring m_sCmdLine;
    tstring m_sTempFileName;
    const boost::function<void(TCHAR*)>& m_fNewLineCallback;
    tstring m_sPrompt;
};
