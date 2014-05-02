#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <fstream>
#include <strstream>
#include <process.h>
#include <boost/function.hpp>
#include "farsdk/plugin.hpp"
#include "farsdk/farcolor.hpp"
#include "miscutil.h"
#include "winhelpers.h"
#include "plugutil.h"
#include "vcs.h"
#include "regwrap.h"
#include "enforce.h"
#include "traverse.h"
#include "lang.h"

using namespace std;
using namespace boost;

//==========================================================================>>
// Global data
//==========================================================================>>

PluginStartupInfo StartupInfo;
FarStandardFunctions FSF;
HINSTANCE hInstance;           // The DLL module handle
HINSTANCE hResInst;            // Where to load the resources from

const TCHAR *cszPluginName = _T("VCS Assistant");
// {8107EF4F-78C3-4ED5-8A3A-6BE16CC5EB1E}
const GUID PluginGuid = { 0x8107ef4f, 0x78c3, 0x4ed5, { 0x8a, 0x3a, 0x6b, 0xe1, 0x6c, 0xc5, 0xeb, 0x1e } };

TCHAR cszDllName[]   = _T("farvcs.dll");
TCHAR cszCacheFile[] = _T("farvcs.csh");

const TCHAR *PluginMenuStrings[] = { cszPluginName };

// The static vars are used to avoid multiple allocations of two-byte blocks in GetFindData

TCHAR *cszEmptyLine = _T("");

TCHAR *Stati[] = { _T(""),  _T(""),   _T(""),   _T(""),  _T(""),  _T(""),  _T(""),  _T(""),  _T(""),  _T(""),  _T(""),  _T(""),  _T(""),   _T(""),  _T(""),  _T(""),
                   _T(""),  _T(""),   _T(""),   _T(""),  _T(""),  _T(""),  _T(""),  _T(""),  _T(""),  _T(""),  _T(""),  _T(""),  _T(""),   _T(""),  _T(""),  _T(""),
                   _T(" "), _T("!"),  _T("\""), _T("#"), _T("$"), _T("%"), _T("&"), _T("'"), _T("("), _T(")"), _T("*"), _T("+"), _T(","),  _T("-"), _T("."), _T("/"),
                   _T("0"), _T("1"),  _T("2"),  _T("3"), _T("4"), _T("5"), _T("6"), _T("7"), _T("8"), _T("9"), _T(":"), _T(";"), _T("<"),  _T("="), _T(">"), _T("?"),
                   _T("@"), _T("A"),  _T("B"),  _T("C"), _T("D"), _T("E"), _T("F"), _T("G"), _T("H"), _T("I"), _T("J"), _T("K"), _T("L"),  _T("M"), _T("N"), _T("O"),
                   _T("P"), _T("Q"),  _T("R"),  _T("S"), _T("T"), _T("U"), _T("V"), _T("W"), _T("X"), _T("Y"), _T("Z"), _T("["), _T("\\"), _T("]"), _T("^"), _T("_"),
                   _T("`"), _T("a"),  _T("b"),  _T("c"), _T("d"), _T("e"), _T("f"), _T("g"), _T("h"), _T("i"), _T("j"), _T("k"), _T("l"),  _T("m"), _T("n"), _T("o"),
                   _T("p"), _T("q"),  _T("r"),  _T("s"), _T("t"), _T("u"), _T("v"), _T("w"), _T("x"), _T("y"), _T("z"), _T("{"), _T("|"),  _T("}"), _T("~"), _T("") };

//==========================================================================>>
// The list of the directories with dirty files and the list of the
// outdated files
//==========================================================================>>

TSFileSet DirtyDirs;
TSFileSet OutdatedFiles;

//==========================================================================>>
// DllMain is necessary to get and store the module handle to access the
// resources later
//==========================================================================>>

BOOL WINAPI DllMain(HINSTANCE hInstDll, DWORD dwReason, LPVOID)
{
    if (dwReason == DLL_PROCESS_ATTACH)
        hInstance = hResInst = hInstDll;

    return TRUE;
}

//==========================================================================>>
// The VCS plugin proper
//==========================================================================>>

class VcsPlugin : private noncopyable
{
public:
    struct PluginSettings
    {
        bool bAutomaticMode; // Auto-start/stop plugin when entering/leaving a VCS-controlled directory
        char cPanelMode;     // Panel mode to use when starting plugin. Ranges '0'-'9'. '\0' means "no change".

        // CVS

        unsigned int nCompressionLevel; // Compression level (-z option). Ranges 0-9;

        void Load();
        void Save() const;

    private:
        static const TCHAR * const cszSettingsKey;
    };

    struct Cache
    {
        Cache(const tstring& sCacheFile = CatPath(GetLocalAppDataFolder().c_str(), cszCacheFile)) : sCacheFile_(sCacheFile) {}

        void Load()
        {
            ifstream is( sCacheFile_.c_str(), ios::binary );
            
            if ( !is )
                return;

            try
            {
                boost::archive::text_iarchive ia( is );

                ia >> DirtyDirs;
                ia >> OutdatedFiles;
            }
            catch(...)
            {
                DirtyDirs.Clear();
                OutdatedFiles.Clear();
            }
        }

        void Save() const
        {
            ofstream os( sCacheFile_.c_str(), ios::binary );
            
            if ( !os )
                return;

            boost::archive::text_oarchive oa( os );

            oa << const_cast<const TSFileSet&>( DirtyDirs );
            oa << const_cast<const TSFileSet&>( OutdatedFiles );
        }

    private:
        tstring sCacheFile_;
    };

public:
    explicit VcsPlugin(tstring currentDirectory, tstring currentItem)
    {
        curDir = currentDirectory;
        itemToStart = currentItem;

        ColumnTitles1[0] = GetMsg(M_ColumnName);
        ColumnTitles1[1] = GetMsg(M_ColumnS);
        ColumnTitles1[2] = GetMsg(M_ColumnName);
        ColumnTitles1[3] = GetMsg(M_ColumnS);
        ColumnTitles1[4] = GetMsg(M_ColumnName);
        ColumnTitles1[5] = GetMsg(M_ColumnS);

        ColumnTitles2[0] = GetMsg(M_ColumnName);
        ColumnTitles2[1] = GetMsg(M_ColumnS);
        ColumnTitles2[2] = GetMsg(M_ColumnName);
        ColumnTitles2[3] = GetMsg(M_ColumnS);

        ColumnTitles3[0] = GetMsg(M_ColumnName);
        ColumnTitles3[1] = GetMsg(M_ColumnS);
        ColumnTitles3[2] = GetMsg(M_ColumnRev);
        ColumnTitles3[3] = GetMsg(M_ColumnOpt);
        ColumnTitles3[4] = GetMsg(M_ColumnTags);

        ColumnTitles4[0] = GetMsg(M_ColumnName);
        ColumnTitles4[1] = GetMsg(M_ColumnS);
        ColumnTitles4[2] = GetMsg(M_ColumnRev);
        ColumnTitles4[3] = GetMsg(M_ColumnOpt);
        ColumnTitles4[4] = GetMsg(M_ColumnTags);

        ::memset(PanelModesArray, 0, sizeof PanelModesArray);

        PanelModesArray[1].ColumnTypes = _T("NO,C0,NO,C0,NO,C0");
        _sntprintf_s(szColumnWidths1, _TRUNCATE, _T("0,1,0,1,0,1"));
        PanelModesArray[1].ColumnWidths = szColumnWidths1;
        PanelModesArray[1].ColumnTitles = ColumnTitles1;
        PanelModesArray[1].StatusColumnTypes = _T("NOR,S,D,T");
        PanelModesArray[1].StatusColumnWidths = _T("0,6,0,5");
        PanelModesArray[1].Flags = PMFLAGS_ALIGNEXTENSIONS;

        PanelModesArray[2].ColumnTypes = _T("NO,C0,NO,C0");
        _sntprintf_s(szColumnWidths2, _TRUNCATE, _T("0,1,0,1"));
        PanelModesArray[2].ColumnWidths = szColumnWidths2;
        PanelModesArray[2].ColumnTitles = ColumnTitles2;
        PanelModesArray[2].StatusColumnTypes = _T("NOR,S,D,T");
        PanelModesArray[2].StatusColumnWidths = _T("0,6,0,5");

        PanelModesArray[3].ColumnTypes = _T("NO,C0,C1,C2,C3");
        _sntprintf_s(szColumnWidths3, _TRUNCATE, _T("0,1,%d,%d,0"), nRevColumnWidth, nOptColumnWidth);
        PanelModesArray[3].ColumnWidths = szColumnWidths3;
        PanelModesArray[3].ColumnTitles = ColumnTitles3;
        PanelModesArray[3].StatusColumnTypes = _T("NOR,S,D,T");
        PanelModesArray[3].StatusColumnWidths = _T("0,6,0,5");

        PanelModesArray[4].ColumnTypes = _T("NO,C0,C1,C2,C3");
        _sntprintf_s(szColumnWidths4, _TRUNCATE, _T("0,1,%d,%d,0"), nRevColumnWidth, nOptColumnWidth);
        PanelModesArray[4].ColumnWidths = szColumnWidths4;
        PanelModesArray[4].ColumnTitles = ColumnTitles4;
        PanelModesArray[4].StatusColumnTypes = _T("NOR,S,D,T");
        PanelModesArray[4].StatusColumnWidths = _T("0,6,0,5");
    }

    void GetOpenPanelInfo(OpenPanelInfo *pinfo);
    intptr_t SetDirectory(const SetDirectoryInfo *pinfo);
    intptr_t GetFindData(GetFindDataInfo *pinfo);
    void FreeFindData(const FreeFindDataInfo *pinfo);
    intptr_t ProcessPanelInput(ProcessPanelInputInfo *pinfo);
    intptr_t ProcessPanelEvent(const ProcessPanelEventInfo *pinfo);

    virtual ~VcsPlugin() {}

    static void StartMonitoringThread() { MonitoringThread.Start(&StartupInfo); }
    static void StopMonitoringThread() { MonitoringThread.Stop(1000); }

private:
    tstring curDir;
    tstring itemToStart; // Where to position the cursor when starting

    // Members to be used in GetOpenPanelInfo

    TCHAR szPanelTitle[MAX_PATH];

    const TCHAR *ColumnTitles1[6];
    const TCHAR *ColumnTitles2[4];
    const TCHAR *ColumnTitles3[5];
    const TCHAR *ColumnTitles4[5];

    TCHAR szColumnWidths1[30];
    TCHAR szColumnWidths2[30];
    TCHAR szColumnWidths3[30];
    TCHAR szColumnWidths4[30];

    PanelMode PanelModesArray[10];

    enum { nOptColumnWidth = 5, nRevColumnWidth = 11 }; // Revision column width

    void DecoratePanelItem(PluginPanelItem& pi, const VcsEntry& entry, const tstring& sTag);

    // Threading for automatic mode

    static Thread MonitoringThread;
    static unsigned int MonitoringThreadRoutine(void *pStartupInfo, HANDLE hTerminateEvent);

    vector<TempFile> TempFiles;
};

//==========================================================================>>
// Configuration settings
//==========================================================================>>

VcsPlugin::PluginSettings Settings;
VcsPlugin::Cache          Cache;

const TCHAR * const VcsPlugin::PluginSettings::cszSettingsKey = _T("HKEY_CURRENT_USER\\Software\\FAR\\Plugins\\FarVCS");

void VcsPlugin::PluginSettings::Load()
{
    RegWrap rkey(cszSettingsKey);
    bAutomaticMode = rkey.ReadDword(_T("bAutomaticMode")) != 0;

    tstring sPanelMode = rkey.ReadString(_T("cPanelMode"));
    cPanelMode = sPanelMode.empty() || !isdigit(sPanelMode[0]) ? 0 : sPanelMode[0];

    // CVS

    nCompressionLevel = rkey.ReadDword(_T("nCompressionLevel"));
}

void VcsPlugin::PluginSettings::Save() const
{
    RegWrap rkey(cszSettingsKey, KEY_ALL_ACCESS);
    rkey.WriteDword (_T("bAutomaticMode"), bAutomaticMode);
    rkey.WriteString(_T("cPanelMode"), tstring(1, cPanelMode));

    // CVS

    rkey.WriteDword(_T("nCompressionLevel"), nCompressionLevel);
}

//==========================================================================>>
// Exported interface functions and forwarders
//==========================================================================>>

void ApplyAutomaticModeFromSettings()
{
    if (Settings.bAutomaticMode)
    {
        // Warn the user if there is no hotkey assigned to our plugin in the plugin menu: we won't be able to autostart the panel

        if (!GetPluginHotkey(cszDllName))
            MsgBoxWarning(cszPluginName, _T("%s"), GetMsg(M_MsgNoHotkey));

        VcsPlugin::StartMonitoringThread();
    }
    else
    {
        VcsPlugin::StopMonitoringThread();
    }
}

void WINAPI SetStartupInfoW(const PluginStartupInfo *pinfo)
{
    if (pinfo->StructSize < sizeof(PluginStartupInfo))
        return;

    StartupInfo = *pinfo;
    FSF = *pinfo->FSF;
    StartupInfo.FSF = &FSF;

    Settings.Load();
    ::Cache.Load();
    ApplyAutomaticModeFromSettings();
}

void WINAPI GetPluginInfoW(PluginInfo *pinfo)
{
    pinfo->StructSize = sizeof PluginInfo;
    pinfo->Flags = PF_PRELOAD;

    pinfo->PluginMenu.Guids = &PluginGuid; // !!!
    pinfo->PluginMenu.Strings = PluginMenuStrings;
    pinfo->PluginMenu.Count = _countof(PluginMenuStrings);

    pinfo->PluginConfig.Guids = &PluginGuid; // !!!
    pinfo->PluginConfig.Strings = PluginMenuStrings;
    pinfo->PluginConfig.Count = _countof(PluginMenuStrings);
}

tstring GetPanelDir(HANDLE hPanel = PANEL_ACTIVE)
{
    intptr_t requiredSize = StartupInfo.PanelControl(hPanel, FCTL_GETPANELDIRECTORY, 0, nullptr);

    if (requiredSize <= 0)
        return _T("");

    unique_ptr<char[]> pbuf{ new char[requiredSize] }; // Replace with make_unique as soon as compiler supports it.
    StartupInfo.PanelControl(hPanel, FCTL_GETPANELDIRECTORY, requiredSize, pbuf.get());

    return reinterpret_cast<FarPanelDirectory*>(pbuf.get())->Name;
}

tstring GetCurrentItem(HANDLE hPanel = PANEL_ACTIVE)
{
    intptr_t requiredSize = StartupInfo.PanelControl(hPanel, FCTL_GETCURRENTPANELITEM, 0, nullptr);

    if (requiredSize <= 0)
        return _T("");

    unique_ptr<char[]> pbuf{ new char[requiredSize] }; // Replace with make_unique as soon as compiler supports it.

    FarGetPluginPanelItem gpi
    {
        sizeof FarGetPluginPanelItem,
        requiredSize,
        reinterpret_cast<PluginPanelItem*>(pbuf.get())
    };

    StartupInfo.PanelControl(hPanel, FCTL_GETCURRENTPANELITEM, 0, &gpi);
    return gpi.Item->FileName;
}

HANDLE WINAPI OpenW(const struct OpenInfo *pinfo)
{
    if (pinfo->StructSize < sizeof(OpenInfo))
        return nullptr;

    if (pinfo->OpenFrom != OPEN_FILEPANEL &&
        pinfo->OpenFrom != OPEN_PLUGINSMENU &&
        pinfo->OpenFrom != OPEN_SHORTCUT)
        return nullptr;

    FarPanelDirectory fpd{ sizeof FarPanelDirectory };

    return new VcsPlugin(GetPanelDir(), GetCurrentItem()); // Deleted in ClosePlugin
}

void WINAPI ClosePanelW(const ClosePanelInfo *pinfo)
{
    delete reinterpret_cast<VcsPlugin*>(pinfo->hPanel);
}

intptr_t WINAPI SetDirectoryW     (const SetDirectoryInfo *pinfo)      { return reinterpret_cast<VcsPlugin*>(pinfo->hPanel)->SetDirectory(pinfo);      }
void     WINAPI GetOpenPanelInfoW (OpenPanelInfo *pinfo)               {        reinterpret_cast<VcsPlugin*>(pinfo->hPanel)->GetOpenPanelInfo(pinfo);  }
intptr_t WINAPI GetFindDataW      (GetFindDataInfo *pinfo)             { return reinterpret_cast<VcsPlugin*>(pinfo->hPanel)->GetFindData(pinfo);       }
void     WINAPI FreeFindDataW     (const FreeFindDataInfo *pinfo)      {        reinterpret_cast<VcsPlugin*>(pinfo->hPanel)->FreeFindData(pinfo);      }
intptr_t WINAPI ProcessPanelInputW(ProcessPanelInputInfo *pinfo)       { return reinterpret_cast<VcsPlugin*>(pinfo->hPanel)->ProcessPanelInput(pinfo); }
intptr_t WINAPI ProcessPanelEventW(const ProcessPanelEventInfo *pinfo) { return reinterpret_cast<VcsPlugin*>(pinfo->hPanel)->ProcessPanelEvent(pinfo); }

//!!! Revise and uncomment
//intptr_t WINAPI ConfigureW(const ConfigureInfo *pinfo)
//{
//    // We expect the settings having been already loaded during the preceding call to SetStartupInfo
//
//    InitDialogItem InitItems[] =
//    {
//        /* 0 */ { DI_DOUBLEBOX, 3,1,52,7,  0,0, 0,                            0, const_cast<char*>( cszPluginName )              },
//        /* 1 */ { DI_CHECKBOX,  5,2,0,0,   0,0, 0,                            0, const_cast<char*>( GetMsg( M_AutomaticMode ) )  },
//        /* 2 */ { DI_TEXT,      5,3,0,0,   0,0, DIF_BOXCOLOR | DIF_SEPARATOR, 0, ""                                              },
//        /* 3 */ { DI_FIXEDIT,   6,4,6,4,   0,0, 0,                            0, ""                                              },
//        /* 4 */ { DI_TEXT,      8,4,50,4,  0,0, 0,                            0, const_cast<char*>( GetMsg( M_PanelModeLabel ) ) },
//        /* 5 */ { DI_TEXT,      5,5,0,0,   0,0, DIF_BOXCOLOR | DIF_SEPARATOR, 0, ""                                              },
//        /* 6 */ { DI_BUTTON,    0,6,0,0,   0,0, DIF_CENTERGROUP,              1, const_cast<char*>( GetMsg( M_Ok ) )             },
//        /* 7 */ { DI_BUTTON,    0,6,0,0,   0,0, DIF_CENTERGROUP,              0, const_cast<char*>( GetMsg( M_Cancel ) )         }
//    };
//
//    enum { dwItemNum = sizeof InitItems / sizeof *InitItems };
//
//    struct FarDialogItem DialogItems[dwItemNum];
//    InitDialogItems( InitItems, DialogItems, dwItemNum );
//
//    DialogItems[1].Selected = Settings.bAutomaticMode;
//    DialogItems[3].Data[0] = Settings.cPanelMode;
//    DialogItems[3].Data[1] = 0;
//
//    int ExitCode = StartupInfo.Dialog( StartupInfo.ModuleNumber, -1, -1, 56, 9, 0, DialogItems, dwItemNum );
//    if ( ExitCode != 6 )
//        return FALSE;
//
//    Settings.bAutomaticMode = DialogItems[1].Selected != 0;
//    Settings.cPanelMode = DialogItems[3].Data[0] && isdigit(DialogItems[3].Data[0]) ? DialogItems[3].Data[0] : 0;
//
//    Settings.Save();
//
//    ApplyAutomaticModeFromSettings();
//    return TRUE;
//}

void WINAPI ExitFARW(const ExitInfo *)
{
    if (Settings.bAutomaticMode)
        VcsPlugin::StopMonitoringThread();
}

/// <summary>
/// Called by Far to change the current directory of the plugin's panel.
/// </summary>
intptr_t VcsPlugin::SetDirectory(const SetDirectoryInfo *pinfo)
{
    tstring newDir = CatPath(curDir.c_str(), pinfo->Dir);

    // Check if the directory acutally exists

    DWORD dwAttributes = ::GetFileAttributes(newDir.c_str());
    if (dwAttributes == (DWORD)-1 || (dwAttributes | FILE_ATTRIBUTE_DIRECTORY) == 0)
        return 0;

    curDir = newDir;
    ::SetCurrentDirectory(curDir.c_str());

    if (::Settings.bAutomaticMode && !IsVcsDir(newDir))
        StartupInfo.PanelControl(PANEL_ACTIVE, FCTL_CLOSEPANEL, 0, reinterpret_cast<void*>(const_cast<TCHAR*>(curDir.c_str())));

    return 1;
}

/// <summary>
/// Called by Far before opening a plugin panel.
/// </summary>
void VcsPlugin::GetOpenPanelInfo(OpenPanelInfo *pinfo)
{
    pinfo->StructSize = sizeof OpenPanelInfo;
    pinfo->Flags = OPIF_REALNAMES | OPIF_SHOWPRESERVECASE | OPIF_EXTERNALGET | OPIF_EXTERNALPUT | OPIF_EXTERNALDELETE | OPIF_EXTERNALMKDIR;
    pinfo->CurDir = curDir.c_str(); // ??? Can we specify 0 here?

    tstring sLabel = !IsVcsDir(curDir) ? _T("VCS") : GetVcsData(curDir)->getTag();

    if (sLabel.empty())
        sLabel = _T("TRUNK");
    
    _sntprintf_s(szPanelTitle, _TRUNCATE, _T(" [%s] %s "), sLabel.c_str(), curDir.c_str());
    pinfo->PanelTitle = szPanelTitle;

    pinfo->PanelModesArray = PanelModesArray;
    pinfo->PanelModesNumber = _countof(PanelModesArray);

    pinfo->StartPanelMode = Settings.cPanelMode;
    pinfo->StartSortMode = SM_DESCR;
    pinfo->StartSortOrder = 0;
}

//==========================================================================>>
// Decorate the panel item with the VCS-related data
//==========================================================================>>

void VcsPlugin::DecoratePanelItem(PluginPanelItem& pi, const VcsEntry& entry, const tstring& sTag)
{
    if (_tcscmp(pi.FileName, _T("..")) == 0)
        return;

    // Allocate storage for and fill custom columns values

    const int nCustomColumns = 4;

    unique_ptr<TCHAR*[]> pCols{ new TCHAR*[nCustomColumns] };

    for (int i = 0; i < nCustomColumns; ++i)
        pCols[i] = cszEmptyLine;

    EVcsStatus fs = entry.status;

    char cStatus = fs == fsAdded     ? 'A' :
                   fs == fsRemoved   ? 'R' :
                   fs == fsConflict  ? 'C' :
                   fs == fsModified  ? 'M' :
                   fs == fsOutdated  ? '*' :
                   fs == fsNonVcs    ? '?' :
                   fs == fsIgnored   ? '|' :
                   fs == fsAddedRepo ? 'a' :
                   fs == fsGhost     ? '!' :
                                       ' ';

    if (cStatus != ' ')
        pCols[0] = Stati[cStatus];

    pi.Description = Stati[ fs == fsAdded     ? '2' :
                            fs == fsRemoved   ? '3' :
                            fs == fsConflict  ? '0' :
                            fs == fsModified  ? '1' :
                            fs == fsGhost     ? '4' :
                            fs == fsAddedRepo ? '8' :
                            fs == fsNonVcs    ? '9' :
                            fs == fsIgnored   ? '9' :
                                                '6'   ];   // For sorting

    if (!IsVcsFile(fs))
    {
        pi.FileAttributes |= FILE_ATTRIBUTE_TEMPORARY;

        pi.CustomColumnData = pCols.release();
        pi.CustomColumnNumber = nCustomColumns;

        return;
    }

    if (fs == fsGhost || fs == fsAddedRepo || fs == fsRemoved)
        pi.FileAttributes |= (entry.bDir ? FILE_ATTRIBUTE_DIRECTORY : 0) | FILE_ATTRIBUTE_HIDDEN;

    if (IsFileDirty(fs))
        pi.FileAttributes |= FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;

    if (!entry.sRevision.empty())
    {
        pCols[1] = new TCHAR[nRevColumnWidth + 1];
        _sntprintf_s(pCols[1], nRevColumnWidth + 1, _TRUNCATE, _T("%*s"), nRevColumnWidth, fs == fsAdded ? _T("Added") : entry.sRevision.c_str());
        pCols[1][nRevColumnWidth] = 0;
    }

    if (!entry.sOptions.empty())
    {
        pCols[2] = new TCHAR[nOptColumnWidth + 1];
        _sntprintf_s(pCols[2], nOptColumnWidth + 1, _TRUNCATE, _T("%-*s"), nOptColumnWidth, entry.sOptions.c_str());
        pCols[2][nOptColumnWidth] = 0;
    }

    if (entry.bDir) {
        if (!sTag.empty()) {
            pCols[3] = new TCHAR[sTag.length() + 1];
            _tcscpy_s(pCols[3], sTag.length() + 1, sTag.c_str());
        }
    }
    else {
        if (!entry.sTagdate.empty()) {
            pCols[3] = new TCHAR[entry.sTagdate.size() + 1];
            _tcscpy_s(pCols[3], entry.sTagdate.size() + 1, entry.sTagdate.c_str() + (entry.sTagdate.empty() ? 0 : 1));
        }
    }

    pi.CustomColumnData = pCols.release();
    pi.CustomColumnNumber = nCustomColumns;
}

// Return by value -- relying on NRVO
PluginPanelItem W32FindDataToPluginPanelItem(const WIN32_FIND_DATA& findData)
{
    PluginPanelItem pi;
    memset(&pi, 0, sizeof pi);

    pi.CreationTime = findData.ftCreationTime;
    pi.LastAccessTime = findData.ftLastAccessTime;
    pi.LastWriteTime = findData.ftLastWriteTime;
    pi.FileSize = (unsigned long long)findData.nFileSizeHigh << 32 | findData.nFileSizeLow;
    pi.Flags = PPIF_NONE;
    pi.FileAttributes = findData.dwFileAttributes;
    pi.FileName = _tcsdup(findData.cFileName);
    pi.AlternateFileName = _tcsdup(findData.cAlternateFileName);

    return pi;
}

/// <summary>
/// Called by Far to get the file list for the panel.
/// </summary>
intptr_t VcsPlugin::GetFindData(GetFindDataInfo *pinfo)
{
    pinfo->StructSize = sizeof GetFindDataInfo;

    // Read the VCS data (does nothing if not in a VCS-controlled directory)

    boost::intrusive_ptr<IVcsData> pVcsData = GetVcsData(curDir);

    // Enumerate all the file entries in the current directory

    vector<PluginPanelItem> v;
    v.reserve(1000); // Just a guess

    if (pVcsData && pVcsData->IsValid())
    {
        for (const auto& entry : pVcsData->entries())
        {
            PluginPanelItem pi = W32FindDataToPluginPanelItem(entry.second.fileFindData);

            if (entry.second.bDir)
            {
                pi.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
                tstring sFullPathName = CatPath(curDir.c_str(), entry.first.c_str());

                // !!! What is going on here?
                // if (DirtyDirs.ContainsDown(sFullPathName))
                //     entry.second.status = fsModified;
                // else if (OutdatedFiles.ContainsDown(sFullPathName))
                //     entry.second.status = fsOutdated;
            }

            //array_strcpy( pi.FindData.cFileName, strcmp(p->first.c_str(),"..") == 0 ? ".." : CatPath(szCurDir,p->first.c_str()).c_str() );
            DecoratePanelItem(pi, entry.second, pVcsData->getTag());
            v.push_back( pi );
        }
    }
    else
    {
        for (dir_iterator p = dir_iterator(curDir, true); p != dir_iterator(); ++p)
        {
            if (_tcscmp(p->cFileName, _T(".")) == 0)
                continue;

            PluginPanelItem pi = W32FindDataToPluginPanelItem(*p);
            //array_strcpy(pi.FindData.cFileName, strcmp(p->cFileName, "..") == 0 ? ".." : CatPath(szCurDir, p->cFileName).c_str());
            v.push_back( pi );
        }
    }

    // Convert the data into old-fashioned format required by FAR

    if (!v.empty())
    {
        pinfo->PanelItem = new PluginPanelItem[v.size()];
        memcpy(pinfo->PanelItem, &v[0], v.size() * sizeof PluginPanelItem);
        pinfo->ItemsNumber = v.size();
    }

    if (pVcsData == nullptr)
        return 1;

    // Make sure the non-existing files are not selected

    /* !!! Review and uncomment
    PanelInfo pinfo;
    StartupInfo.Control(INVALID_HANDLE_VALUE, FCTL_GETPANELINFO, &pinfo );

    bool bSomeSelectionChanged = false;

    if ( pinfo.PanelType == PTYPE_FILEPANEL && pinfo.Plugin && pinfo.SelectedItemsNumber >= 1 )
    {
        for ( int i = 0; i < pinfo.ItemsNumber; ++i )
        {
            if ( (pinfo.PanelItems[i].Flags & PPIF_SELECTED) == 0 )
                continue;

            VcsEntries::const_iterator p = apVcsData->entries().find( ExtractFileName( pinfo.PanelItems[i].FindData.cFileName ) );

            if ( p == apVcsData->entries().end() )
                continue;

            pinfo.PanelItems[i].Flags &= ~PPIF_SELECTED;
            bSomeSelectionChanged = true;
        }

        if ( bSomeSelectionChanged )
            StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_SETSELECTION, &pinfo );
    }
    */
    return 1;
}

void VcsPlugin::FreeFindData(const FreeFindDataInfo *pinfo)
{
    for (int i = 0; i < pinfo->ItemsNumber; ++i)
    {
        for (int j = 1; j < pinfo->PanelItem[i].CustomColumnNumber; ++j) // The zeroth item is the status and it's static
            if (pinfo->PanelItem[i].CustomColumnData[j] != cszEmptyLine)
                delete[] pinfo->PanelItem[i].CustomColumnData[j];

        delete[] pinfo->PanelItem[i].CustomColumnData;

        free(reinterpret_cast<void*>(const_cast<TCHAR*>(pinfo->PanelItem[i].FileName)));
        free(reinterpret_cast<void*>(const_cast<TCHAR*>(pinfo->PanelItem[i].AlternateFileName)));
    }

    delete[] pinfo->PanelItem;
}

//==========================================================================>>
// Handle the FE_REDRAW event to keep intact the current cursor position
// when starting the plugin panel
//==========================================================================>>

intptr_t VcsPlugin::ProcessPanelEvent(const ProcessPanelEventInfo *pinfo)
{
    /* !!! Revise and uncomment
    if (pinfo->Event == FE_REDRAW)
    {
        if (itemToStart.empty())
            return FALSE;

        tstring sItemToStart{ itemToStart };
        itemToStart.clear();

        PanelInfo pinfo;
        StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_GETPANELINFO, &pinfo );

        if ( pinfo.PanelType != PTYPE_FILEPANEL )
            return FALSE;

        if ( pinfo.ItemsNumber == 0 )
            return FALSE;

        int i = 0;

        for ( ; i < pinfo.ItemsNumber; ++i )
            if ( _stricmp( ExtractFileName(pinfo.PanelItems[i].FindData.cFileName).c_str(), sItemToStart.c_str() ) == 0 )
                break;

        if ( i >= pinfo.ItemsNumber )
            return FALSE;

        PanelRedrawInfo pri = { i, pinfo.TopPanelItem };
        StartupInfo.Control( this, FCTL_REDRAWPANEL, &pri );
    }
    */
    return FALSE;
}

/* !!! Review and uncomment
//==========================================================================>>
// We have to override the processing of Ctrl+Enter and Ctrl+Insert keys
// because of the full pathnames in our PluginPanelItem structures
//==========================================================================>>

intptr_t VcsPlugin::ProcessPanelInput(ProcessPanelInputInfo *pinfo)
{
    if ( Key & PKF_PREPROCESS )
        return FALSE;

    // Check if we are on a file panel of a plugin

    PanelInfo pi;
    StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_GETPANELINFO, &pi );

    if ( pi.PanelType != PTYPE_FILEPANEL || !pi.Plugin || pi.ItemsNumber == 0 )
        return FALSE;

    bool bCtrl      =  (ControlState & PKF_CONTROL) && ~(ControlState & PKF_ALT) && ~(ControlState & PKF_SHIFT);
    bool bCtrlAlt   =  (ControlState & PKF_CONTROL) &&  (ControlState & PKF_ALT) && ~(ControlState & PKF_SHIFT);
    bool bCtrlShift =  (ControlState & PKF_CONTROL) && ~(ControlState & PKF_ALT) &&  (ControlState & PKF_SHIFT);
    bool bAltShift  = ~(ControlState & PKF_CONTROL) &&  (ControlState & PKF_ALT) &&  (ControlState & PKF_SHIFT);

    const char *szCurFile = pi.PanelItems[pi.CurrentItem].FindData.cFileName;

    if ( bCtrl && Key == VK_RETURN  ) // Ctrl+Enter
    {
        // Get the current file

        if ( strcmp( szCurFile, ".." ) == 0 )
            return FALSE;

        // Separate the file name, add quotes if necessary, insert into the command line

        StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_INSERTCMDLINE,
                             (void*)(QuoteIfNecessary(ExtractFileName(szCurFile))+' ').c_str() );
        return TRUE;
    }
    else if ( bCtrl && Key == VK_INSERT ) // Ctrl+Insert
    {
        // Pass through to FAR if the command line is not empty

        char szBuf[2048]; // The documentation says it should be more than 1024

        if ( StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_GETCMDLINE, &szBuf ) && *szBuf != 0 )
            return FALSE;

        // Get the panel info to iterate over the selected items

        if ( pi.SelectedItemsNumber < 1 )
            return FALSE;

        // Build the list of the selected filenames

        string sClipboard;

        for ( int i = 0; i < pi.SelectedItemsNumber; ++i )
        {
            string sPathName = pi.SelectedItems[i].FindData.cFileName;

            if ( sPathName == ".." )
                continue;

            // Add the filename (quoted if necessary) to the list with the CRLF separator

            if ( !sClipboard.empty() )
                 sClipboard += "\r\n";

            sClipboard += QuoteIfNecessary( ExtractFileName( sPathName ) );
        }

        // Put the list to the clipboard

        if ( sClipboard.empty() )
            return FALSE;

        return FSF.CopyToClipboard( sClipboard.c_str() );
    }
    else if ( bCtrl && Key == 'R' ) // Ctrl+R
    {
        Traversal( cszPluginName, szCurDir ).Execute();
        ::Cache.Save();
        return FALSE;
    }
    else if ( bAltShift && Key == VK_F11 ) // Alt+Shift+F11
    {
        ::Settings.bAutomaticMode = !Settings.bAutomaticMode;
        ApplyAutomaticModeFromSettings();
        if ( !Settings.bAutomaticMode )
            StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_CLOSEPLUGIN, szCurDir );

        return TRUE;
    }
    else if ( bAltShift && Key == VK_F10 ) // Alt+Shift+F10
    {
        StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_CLOSEPLUGIN, szCurDir );
        return TRUE;
    }
    else if ( bAltShift && (Key == VK_F3 || Key == VK_F4) ) // Alt+Shift+F3 or Alt+Shift+F4
    {
        boost::intrusive_ptr<IVcsData> apVcsData = GetVcsData( szCurDir );

        if ( !apVcsData || !apVcsData->IsValid() || !IsVcsFile(pi.PanelItems[pi.CurrentItem].FindData,*apVcsData) && !OutdatedFiles.ContainsEntry(szCurFile) )
            return FALSE;

        TempFile tempFile;

        if ( !apVcsData->Annotate( szCurFile, tempFile.GetName() ) )
            return FALSE;

        string sTitle = sformat( "[CVS annotate] %s", szCurFile );

        if ( Key == VK_F3 )
            StartupInfo.Viewer( tempFile.GetName().c_str(),
                                sTitle.c_str(),
                                0, 0, -1, -1,
                                VF_DELETEONCLOSE | VF_DISABLEHISTORY | VF_ENABLE_F6 );
        else // Key == VK_F4
            StartupInfo.Editor( tempFile.GetName().c_str(),
                                sTitle.c_str(),
                                0, 0, -1, -1,
                                EF_DELETEONCLOSE | EF_DISABLEHISTORY | EF_ENABLE_F6,
                                1, 1 );
        return TRUE;
    }
    else if ( (bCtrlAlt || bCtrlShift) && (Key == VK_F5 || Key == VK_F6) ) // Alt/Ctrl + Shift + F5/F6
    {
        boost::intrusive_ptr<IVcsData> apVcsData = GetVcsData( szCurDir );

        bool bLocal = bCtrlAlt;

        if ( Key == VK_F5 )
        {
            OutdatedFiles.RemoveFilesOfDir( szCurDir, !bLocal );
            apVcsData->UpdateStatus( bLocal );
        }
        else
        {
            if ( apVcsData->Update( bLocal ) )
                OutdatedFiles.RemoveFilesOfDir( szCurDir, !bLocal );
        }

        if ( !bLocal )
            Traversal( cszPluginName, szCurDir ).Execute();

        ::Cache.Save();

        StartupInfo.Control( this, FCTL_UPDATEPANEL, 0 );
        StartupInfo.Control( this, FCTL_REDRAWPANEL, 0 );

        return TRUE;
    }
    else if ( bCtrl && Key == VK_OEM_PLUS ) // Ctrl+=
    {
        boost::intrusive_ptr<IVcsData> apVcsData = GetVcsData( szCurDir );

        if ( !apVcsData || !apVcsData->IsValid() || !IsVcsFile(pi.PanelItems[pi.CurrentItem].FindData,*apVcsData) )
            return FALSE;

        VcsEntries::const_iterator p = apVcsData->entries().find( ExtractFileName(szCurFile).c_str() );

        if ( p == apVcsData->entries().end() || p->second.sRevision.empty() )
            return FALSE;
            
        TempFile tempFile( sformat( "%s_%s", CatPath(GetTempPath().c_str(),ExtractFileName(szCurFile).c_str()).c_str(), p->second.sRevision.c_str() ).c_str() );

        if ( !apVcsData->GetRevisionTemp( szCurFile, p->second.sRevision, tempFile.GetName() ) )
            return FALSE;

        W32Handle( ExecuteConsoleNoWait( szCurDir,
                                         sformat("merge.exe %s %s", QuoteIfNecessary(tempFile.GetName()).c_str(), QuoteIfNecessary(szCurFile).c_str()).c_str(),
                                         0, 0 ) );

        TempFiles.push_back( tempFile );

        return TRUE;
    }
    else if ( bCtrlAlt && Key == VK_F9 ) // Ctrl+Alt+F9
    {
        ::Cache.Save();
        return TRUE;
    }
    else if ( bCtrlShift && Key == VK_F9 ) // Ctrl+Shift+F9
    {
        ::Cache.Load();

        StartupInfo.Control( this, FCTL_UPDATEPANEL, 0 );
        StartupInfo.Control( this, FCTL_REDRAWPANEL, 0 );

        return TRUE;
    }

    return FALSE;
}
*/
//// !!! Review and uncomment
////==========================================================================>>
//// Directory monitoring thread routine. Starts the VCS plugin panel as soon
//// as we enter a VCS-controlled directory
////==========================================================================>>
//
//Thread VcsPlugin::MonitoringThread( VcsPlugin::MonitoringThreadRoutine );
//
//unsigned int VcsPlugin::MonitoringThreadRoutine( void *pStartupInfo, HANDLE hTerminateEvent )
//{
//    if ( pStartupInfo == 0 )
//        return 0;
//
//    static PluginStartupInfo I;
//    I = *static_cast<PluginStartupInfo*>( pStartupInfo ); // Note assignment, not initialization
//
//    static vector<unsigned short> CharBuf( nMaxConsoleWidth * nMaxConsoleHeight ); // Character buffer used to scan the console screen
//
//    HANDLE ObjectsToWait[] = { hTerminateEvent }; // WaitForMultipleObjects is used to avoid message processing problems -- see MSDN
//
//    for ( ; ; )
//    {
//        if ( ::WaitForMultipleObjects( array_size(ObjectsToWait), ObjectsToWait, false, 200 ) != WAIT_TIMEOUT ) // Thread termination requested or something unexpected happened
//            return 0; // Exit the thread
//
//        if ( !::Settings.bAutomaticMode )
//            continue;
//
//        PanelInfo pi;
//        StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_GETPANELSHORTINFO, &pi );
//
//        if ( pi.PanelType != PTYPE_FILEPANEL || pi.Plugin || !IsVcsDir( pi.CurDir ) )
//            continue;
//
//        // Dirty hack to check that no information/menu/dialog is active
//
//        HANDLE hConsoleOutput = ::GetStdHandle( STD_OUTPUT_HANDLE );
//
//        if ( !hConsoleOutput )
//            continue;
//
//        CONSOLE_SCREEN_BUFFER_INFO screenBufInfo;
//        if ( !::GetConsoleScreenBufferInfo( hConsoleOutput, &screenBufInfo ) )
//            continue;
//
//        unsigned long dwScreenBufSize = screenBufInfo.dwSize.X * screenBufInfo.dwSize.Y;
//        if ( dwScreenBufSize == 0 || dwScreenBufSize > CharBuf.capacity() )
//            continue;
//
//        COORD c = { 0, 0 };
//        unsigned long dwRead;
//        if ( !::ReadConsoleOutputCharacterW( hConsoleOutput, (LPWSTR)&CharBuf[0], dwScreenBufSize, c, &dwRead ) || dwRead != dwScreenBufSize )
//            continue;
//
//        // It only makes sense to start the plugin if there are file panels (at least one) currently displayed.
//        // We detect file panels by box drawing characters in the first and the second rows of the screen.
//
//        unsigned short * const cpFirstRowBegin  = &CharBuf[0];
//        unsigned short * const cpFirstRowEnd    = cpFirstRowBegin + screenBufInfo.dwSize.X;
//        unsigned short * const cpSecondRowBegin = cpFirstRowEnd;
//        unsigned short * const cpScreenEnd      = cpFirstRowBegin + dwScreenBufSize;
//
//        unsigned short *pTopLeftCorner1 = *cpFirstRowBegin == 0x2554 || *cpFirstRowBegin == L'[' /*Bg window number*/ ? cpFirstRowBegin : cpFirstRowEnd;
//        unsigned short *pTopLeftCorner2 = find( cpFirstRowBegin+1, cpFirstRowEnd, 0x2554 );
//
//        unsigned short *pTopRightCorner1 = find( cpFirstRowBegin+1, pTopLeftCorner2, 0x2557 );
//        unsigned short *pTopRightCorner2 = cpFirstRowEnd[-1] == 0x2557 || iswdigit(cpFirstRowEnd[-1]) /*Clock*/ ? cpFirstRowEnd-1 : cpFirstRowEnd;
//
//        bool bFilePanelsDetected =
//            pTopLeftCorner1 < cpFirstRowEnd && pTopRightCorner1 < cpFirstRowEnd &&
//            cpSecondRowBegin[pTopLeftCorner1-cpFirstRowBegin] == 0x2551 && cpSecondRowBegin[pTopRightCorner1-cpFirstRowBegin] == 0x2551 ||
//
//            pTopLeftCorner2 < cpFirstRowEnd && pTopRightCorner2 < cpFirstRowEnd &&
//            cpSecondRowBegin[pTopLeftCorner2-cpFirstRowBegin] == 0x2551 && cpSecondRowBegin[pTopRightCorner2-cpFirstRowBegin] == 0x2551;
//
//        if ( !bFilePanelsDetected )
//            continue;
//
//        // A top-left box drawing character anywhere below the first row means a menu or dialog is currently active
//
//        if ( find( cpSecondRowBegin, cpScreenEnd, 0x2554 ) < cpScreenEnd )
//            continue;
//
//        // Auto-starting the VCS panel (feasible only if a hotkey for the plugin is defined)
//
//        unsigned char cPluginHotkey = GetPluginHotkey( cszDllName );
//
//        if ( cPluginHotkey == 0 )
//            continue;
//
//        HANDLE hConsoleInput = ::GetStdHandle( STD_INPUT_HANDLE );
//
//        if ( !hConsoleInput )
//            continue;
//
//        INPUT_RECORD recs[] = { { KEY_EVENT, { TRUE,  1,        VK_F11, 0,             0, 0 } },
//                                { KEY_EVENT, { FALSE, 1,        VK_F11, 0,             0, 0 } },
//                                { KEY_EVENT, { TRUE,  1, cPluginHotkey, 0, cPluginHotkey, 0 } },
//                                { KEY_EVENT, { FALSE, 1, cPluginHotkey, 0, cPluginHotkey, 0 } } };
//        DWORD dwWritten;
//        ::WriteConsoleInput( hConsoleInput, recs, array_size(recs), &dwWritten );
//    }
//}
