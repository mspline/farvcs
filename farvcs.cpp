/*****************************************************************************
 File name:  farvcs.cpp
 Project:    FarVCS plugin
 Purpose:    The main source file
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhause
 Dependencies: STL
*****************************************************************************/

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
#pragma warning(disable:4121)
#include "plugin.hpp"
#pragma warning(default:4121)
#include "farcolor.hpp"
#include "miscutil.h"
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

const char *cszPluginName = "VCS Assistant";

char cszDllName[]    = "farvcs.dll";
char cszCacheFile[]  = "farvcs.csh";

char *PluginMenuStrings[] = { const_cast<char*>( cszPluginName ) };

// The static vars are used to avoid multiple allocations of two-byte blocks in GetFindData

char *cszEmptyLine = "";

char *Stati[] = {  "",  "",   "",  "",  "",  "",  "",  "",  "",  "",  "",  "",   "",  "",  "",  "",
                   "",  "",   "",  "",  "",  "",  "",  "",  "",  "",  "",  "",   "",  "",  "",  "",
                  " ", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+",  ",", "-", ".", "/",
                  "0", "1",  "2", "3", "4", "5", "6", "7", "8", "9", ":", ";",  "<", "=", ">", "?",
                  "@", "A",  "B", "C", "D", "E", "F", "G", "H", "I", "J", "K",  "L", "M", "N", "O",
                  "P", "Q",  "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "\\", "]", "^", "_",
                  "`", "a",  "b", "c", "d", "e", "f", "g", "h", "i", "j", "k",  "l", "m", "n", "o",
                  "p", "q",  "r", "s", "t", "u", "v", "w", "x", "y", "z", "{",  "|", "}", "~", "" };

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

BOOL WINAPI DllMain( HINSTANCE hInstDll, DWORD dwReason, LPVOID )
{
    if ( dwReason == DLL_PROCESS_ATTACH )
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
        static const char * const cszSettingsKey;
    };

    struct Cache
    {
        Cache( const string& sCacheFile = CatPath(GetLocalAppDataFolder().c_str(),cszCacheFile) ) : sCacheFile_(sCacheFile) {}

        void Load()
        {
            ifstream is( sCacheFile_.c_str(), ios::binary );
            
            if ( !is )
                return;

            boost::archive::text_iarchive ia( is );

            ia >> DirtyDirs;
            ia >> OutdatedFiles;
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
        string sCacheFile_;
    };

public:
    explicit VcsPlugin( const char *pszCurDir, const char *pszItemToStart )
    {
        array_strcpy( szCurDir, pszCurDir );
        array_strcpy( szItemToStart, pszItemToStart );

        ColumnTitles1[0] = const_cast<char*>( GetMsg( M_ColumnName ) );
        ColumnTitles1[1] = const_cast<char*>( GetMsg( M_ColumnS ) );
        ColumnTitles1[2] = const_cast<char*>( GetMsg( M_ColumnName ) );
        ColumnTitles1[3] = const_cast<char*>( GetMsg( M_ColumnS ) );
        ColumnTitles1[4] = const_cast<char*>( GetMsg( M_ColumnName ) );
        ColumnTitles1[5] = const_cast<char*>( GetMsg( M_ColumnS ) );

        ColumnTitles2[0] = const_cast<char*>( GetMsg( M_ColumnName ) );
        ColumnTitles2[1] = const_cast<char*>( GetMsg( M_ColumnS ) );
        ColumnTitles2[2] = const_cast<char*>( GetMsg( M_ColumnName ) );
        ColumnTitles2[3] = const_cast<char*>( GetMsg( M_ColumnS ) );

        ColumnTitles3[0] = const_cast<char*>( GetMsg( M_ColumnName ) );
        ColumnTitles3[1] = const_cast<char*>( GetMsg( M_ColumnS ) );
        ColumnTitles3[2] = const_cast<char*>( GetMsg( M_ColumnRev ) );
        ColumnTitles3[3] = const_cast<char*>( GetMsg( M_ColumnOpt ) );
        ColumnTitles3[4] = const_cast<char*>( GetMsg( M_ColumnTags ) );

        ::memset( PanelModesArray, 0, sizeof PanelModesArray );

        PanelModesArray[1].ColumnTypes = "NO,C0,NO,C0,NO,C0";
        array_sprintf( szColumnWidths1, "0,1,0,1,0,1" );
        PanelModesArray[1].ColumnWidths = szColumnWidths1;
        PanelModesArray[1].ColumnTitles = ColumnTitles1;
        PanelModesArray[1].FullScreen = FALSE;
        PanelModesArray[1].AlignExtensions = TRUE;
        PanelModesArray[1].StatusColumnTypes = "NOR,S,D,T";
        PanelModesArray[1].StatusColumnWidths = "0,6,0,5";

        PanelModesArray[2].ColumnTypes = "NO,C0,NO,C0";
        array_sprintf( szColumnWidths2, "0,1,0,1" );
        PanelModesArray[2].ColumnWidths = szColumnWidths2;
        PanelModesArray[2].ColumnTitles = ColumnTitles2;
        PanelModesArray[2].FullScreen = FALSE;
        PanelModesArray[2].StatusColumnTypes = "NOR,S,D,T";
        PanelModesArray[2].StatusColumnWidths = "0,6,0,5";

        PanelModesArray[3].ColumnTypes = "NO,C0,C1,C2,C3";
        array_sprintf( szColumnWidths3, "0,1,%d,%d,0", nRevColumnWidth, nOptColumnWidth );
        PanelModesArray[3].ColumnWidths = szColumnWidths3;
        PanelModesArray[3].ColumnTitles = ColumnTitles3;
        PanelModesArray[3].FullScreen = FALSE;
        PanelModesArray[3].StatusColumnTypes = "NOR,S,D,T";
        PanelModesArray[3].StatusColumnWidths = "0,6,0,5";
    }

    void GetOpenPluginInfo( OpenPluginInfo *pInfo );
    int GetFindData( PluginPanelItem **ppItems, int *pItemsNumber, int OpMode );
    void FreeFindData( PluginPanelItem *pItems, int ItemsNumber );
    int SetDirectory( const char *Dir, int OpMode );
    int ProcessKey( int Key, unsigned int ControlState );
    int ProcessEvent( int Event, void *Param );

    virtual ~VcsPlugin() {}

    static void StartMonitoringThread() { MonitoringThread.Start( &StartupInfo ); }
    static void StopMonitoringThread() { MonitoringThread.Stop( 1000 ); }

private:
    char szCurDir[MAX_PATH];

    char szItemToStart[MAX_PATH]; // Where to position the cursor when starting

    // Members to be used in GetOpenPluginInfo

    char szPanelTitle[MAX_PATH];

    char *ColumnTitles1[6];
    char *ColumnTitles2[4];
    char *ColumnTitles3[5];

    char szColumnWidths1[30];
    char szColumnWidths2[30];
    char szColumnWidths3[30];

    PanelMode PanelModesArray[10];

    enum { nOptColumnWidth = 5, nRevColumnWidth = 11 }; // Revision column width

    void DecoratePanelItem( PluginPanelItem& pi, const VcsEntry& entry, const string& sTag );

    // Theading for automatic mode

    static Thread MonitoringThread;
    static unsigned int MonitoringThreadRoutine( void *pStartupInfo, HANDLE hTerminateEvent);
};

//==========================================================================>>
// Configuration settings
//==========================================================================>>

VcsPlugin::PluginSettings Settings;
VcsPlugin::Cache          Cache;

const char * const VcsPlugin::PluginSettings::cszSettingsKey = "HKEY_CURRENT_USER\\Software\\FAR\\Plugins\\FarVCS";

void VcsPlugin::PluginSettings::Load()
{
    RegWrap rkey( cszSettingsKey );
    bAutomaticMode = rkey.ReadDword( "bAutomaticMode" ) != 0;

    string sPanelMode = rkey.ReadString( "cPanelMode" );
    cPanelMode = sPanelMode.empty() || !isdigit(sPanelMode[0]) ? 0 : sPanelMode[0];

    // CVS

    nCompressionLevel = rkey.ReadDword( "nCompressionLevel" );
}

void VcsPlugin::PluginSettings::Save() const
{
    RegWrap rkey( cszSettingsKey, KEY_ALL_ACCESS );
    rkey.WriteDword ( "bAutomaticMode",    bAutomaticMode );
    rkey.WriteString( "cPanelMode",        string(1,cPanelMode) );

    // CVS

    rkey.WriteDword( "nCompressionLevel", nCompressionLevel );
}

//==========================================================================>>
// Exported interface functions and forwarders
//==========================================================================>>

int WINAPI _export GetMinFarVersion()
{
    return MAKEFARVERSION( 1, 70, 0 );
}

void ApplyAutomaticModeFromSettings()
{
    if ( Settings.bAutomaticMode )
    {
        // Warn the user if there is no hotkey assigned to our plugin in the plugin menu: we won't be able to autostart the panel

        if ( !GetPluginHotkey( cszDllName ) )
            StartupInfo.Message( StartupInfo.ModuleNumber,
                                 FMSG_WARNING | FMSG_ALLINONE | FMSG_MB_OK,
                                 0,   // Help topic
                                 (const char * const *)((string(cszPluginName) + "\n" + GetMsg( M_MsgNoHotkey )).c_str()),
                                 0,   // Items number
                                 0 ); // Buttons number

        VcsPlugin::StartMonitoringThread();
    }
    else
    {
        VcsPlugin::StopMonitoringThread();
    }
}

void WINAPI _export SetStartupInfo( const PluginStartupInfo *pPluginStartupInfo )
{
    StartupInfo = *pPluginStartupInfo;
    FSF = *pPluginStartupInfo->FSF;
    StartupInfo.FSF = &FSF;

    Settings.Load();
    ::Cache.Load();
    ApplyAutomaticModeFromSettings();
}

void WINAPI _export GetPluginInfo( PluginInfo *pInfo )
{
    pInfo->StructSize = sizeof PluginInfo;
    pInfo->Flags = PF_PRELOAD;
    pInfo->DiskMenuStrings = 0;
    pInfo->DiskMenuNumbers = 0;
    pInfo->DiskMenuStringsNumber = 0;
    pInfo->PluginMenuStrings = PluginMenuStrings;
    pInfo->PluginMenuStringsNumber = array_size(PluginMenuStrings);
    pInfo->PluginConfigStrings = PluginMenuStrings;
    pInfo->PluginConfigStringsNumber = array_size(PluginMenuStrings);
    pInfo->CommandPrefix = 0;
}

HANDLE WINAPI _export OpenPlugin( int /*OpenFrom*/, int /*Item*/ )
{
    PanelInfo pi;
    StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_GETPANELINFO, &pi );

    if ( pi.PanelType != PTYPE_FILEPANEL )
        return INVALID_HANDLE_VALUE;

    return new VcsPlugin( pi.CurDir, pi.ItemsNumber > 0 ? pi.PanelItems[pi.CurrentItem].FindData.cFileName : "" ); // Deleted in ClosePlugin
}

void WINAPI _export ClosePlugin( HANDLE hPlugin )
{
    delete reinterpret_cast<VcsPlugin*>( hPlugin );
}

int  WINAPI _export SetDirectory     ( HANDLE hPlugin, const char *pszDir, int OpMode )                           { return reinterpret_cast<VcsPlugin*>( hPlugin )->SetDirectory( pszDir, OpMode ); }
void WINAPI _export GetOpenPluginInfo( HANDLE hPlugin, OpenPluginInfo *pInfo )                                    {        reinterpret_cast<VcsPlugin*>( hPlugin )->GetOpenPluginInfo( pInfo ); }
int  WINAPI _export GetFindData      ( HANDLE hPlugin, PluginPanelItem **ppItems, int *pItemsNumber, int OpMode ) { return reinterpret_cast<VcsPlugin*>( hPlugin )->GetFindData( ppItems, pItemsNumber, OpMode ); }
void WINAPI _export FreeFindData     ( HANDLE hPlugin, PluginPanelItem *pItems, int ItemsNumber )                 {        reinterpret_cast<VcsPlugin*>( hPlugin )->FreeFindData( pItems, ItemsNumber ); }
int  WINAPI _export ProcessKey       ( HANDLE hPlugin, int Key, unsigned int ControlState )                       { return reinterpret_cast<VcsPlugin*>( hPlugin )->ProcessKey( Key, ControlState ); }
int  WINAPI _export ProcessEvent     ( HANDLE hPlugin, int Event, void *Param )                                   { return reinterpret_cast<VcsPlugin*>( hPlugin )->ProcessEvent( Event, Param ); }

int WINAPI _export Configure( int /*ItemNumber*/ )
{
    // We expect the settings having been already loaded during the preceding call to SetStartupInfo

    InitDialogItem InitItems[] =
    {
        /* 0 */ { DI_DOUBLEBOX, 3,1,52,7,  0,0, 0,                            0, const_cast<char*>( cszPluginName )              },
        /* 1 */ { DI_CHECKBOX,  5,2,0,0,   0,0, 0,                            0, const_cast<char*>( GetMsg( M_AutomaticMode ) )  },
        /* 2 */ { DI_TEXT,      5,3,0,0,   0,0, DIF_BOXCOLOR | DIF_SEPARATOR, 0, ""                                              },
        /* 3 */ { DI_FIXEDIT,   6,4,6,4,   0,0, 0,                            0, ""                                              },
        /* 4 */ { DI_TEXT,      8,4,50,4,  0,0, 0,                            0, const_cast<char*>( GetMsg( M_PanelModeLabel ) ) },
        /* 5 */ { DI_TEXT,      5,5,0,0,   0,0, DIF_BOXCOLOR | DIF_SEPARATOR, 0, ""                                              },
        /* 6 */ { DI_BUTTON,    0,6,0,0,   0,0, DIF_CENTERGROUP,              1, const_cast<char*>( GetMsg( M_Ok ) )             },
        /* 7 */ { DI_BUTTON,    0,6,0,0,   0,0, DIF_CENTERGROUP,              0, const_cast<char*>( GetMsg( M_Cancel ) )         }
    };

    enum { dwItemNum = sizeof InitItems / sizeof *InitItems };

    struct FarDialogItem DialogItems[dwItemNum];
    InitDialogItems( InitItems, DialogItems, dwItemNum );

    DialogItems[1].Selected = Settings.bAutomaticMode;
    DialogItems[3].Data[0] = Settings.cPanelMode;
    DialogItems[3].Data[1] = 0;

    int ExitCode = StartupInfo.Dialog( StartupInfo.ModuleNumber, -1, -1, 56, 9, 0, DialogItems, dwItemNum );
    if ( ExitCode != 6 )
        return FALSE;

    Settings.bAutomaticMode = DialogItems[1].Selected != 0;
    Settings.cPanelMode = DialogItems[3].Data[0] && isdigit(DialogItems[3].Data[0]) ? DialogItems[3].Data[0] : 0;

    Settings.Save();

    ApplyAutomaticModeFromSettings();
    return TRUE;
}

void WINAPI _export ExitFAR()
{
    if ( Settings.bAutomaticMode )
        VcsPlugin::StopMonitoringThread();
}

//==========================================================================>>
// Accepts an order to change the current directory
//==========================================================================>>

int VcsPlugin::SetDirectory( const char *pszDir, int /*OpMode*/ )
{
    string sNewDir = CatPath( szCurDir, pszDir );

    // Check if the directory acutally exists

    DWORD dwAttributes = ::GetFileAttributes( sNewDir.c_str() );
    if ( dwAttributes == (DWORD)-1 || (dwAttributes | FILE_ATTRIBUTE_DIRECTORY) == 0 )
        return FALSE;

    array_strcpy( szCurDir, sNewDir.c_str() );
    ::SetCurrentDirectory( szCurDir );

    if ( ::Settings.bAutomaticMode && !IsVcsDir( sNewDir ) )
        StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_CLOSEPLUGIN, szCurDir );

    return TRUE;
}

//==========================================================================>>
// GetOpenPluginInfo
//==========================================================================>>

void VcsPlugin::GetOpenPluginInfo( OpenPluginInfo *pInfo )
{
    pInfo->StructSize = sizeof OpenPluginInfo;
    pInfo->Flags = OPIF_USEFILTER | OPIF_USESORTGROUPS | OPIF_USEHIGHLIGHTING | OPIF_SHOWPRESERVECASE |
                   OPIF_REALNAMES | OPIF_EXTERNALGET | OPIF_EXTERNALPUT | OPIF_EXTERNALDELETE | OPIF_EXTERNALMKDIR;
    pInfo->HostFile = 0;
    pInfo->CurDir = szCurDir;
    pInfo->Format = 0;

    string sLabel = !IsVcsDir(szCurDir) ? "VCS" : GetVcsData(szCurDir)->getTag();

    if ( sLabel.empty() )
        sLabel = "TRUNK";
    
    array_sprintf( szPanelTitle, " [%s] %s ", sLabel.c_str(), szCurDir );
    pInfo->PanelTitle = szPanelTitle;

    pInfo->InfoLines = 0;
    pInfo->InfoLinesNumber = 0;
    pInfo->DescrFiles = 0;
    pInfo->DescrFilesNumber = 0;

    pInfo->PanelModesArray = PanelModesArray;
    pInfo->PanelModesNumber = array_size(PanelModesArray);

    pInfo->StartPanelMode = Settings.cPanelMode;
    pInfo->StartSortMode = SM_DESCR;
    pInfo->StartSortOrder = 0;
    pInfo->KeyBar = 0;
    pInfo->ShortcutData = 0;
}

//==========================================================================>>
// Decorate the panel item with the VCS-related data
//==========================================================================>>

void VcsPlugin::DecoratePanelItem( PluginPanelItem& pi, const VcsEntry& entry, const string& sTag )
{
    if ( strcmp( pi.FindData.cFileName, ".." ) == 0 )
        return;

    // Allocate storage for and fill custom columns values

    const int nCustomColumns = 4;

    pi.CustomColumnData = new char*[nCustomColumns];
    pi.CustomColumnNumber = nCustomColumns;
    for ( int i = 0; i < nCustomColumns; ++i )
        pi.CustomColumnData[i] = cszEmptyLine;

    EVcsStatus fs = entry.status;

    char cStatus = fs == fsAdded     ? 'A' :
                   fs == fsRemoved   ? 'R' :
                   fs == fsConflict  ? 'C' :
                   fs == fsModified  ? 'M' :
                   fs == fsOutdated  ? '*' :
                   fs == fsNonVcs    ? '?' :
                   fs == fsAddedRepo ? 'a' :
                   fs == fsGhost     ? '!' :
                                       ' ';

    if ( cStatus != ' ' )
        pi.CustomColumnData[0] = Stati[cStatus];

    pi.Description = Stati[ fs == fsAdded     ? '2' :
                            fs == fsRemoved   ? '3' :
                            fs == fsConflict  ? '0' :
                            fs == fsModified  ? '1' :
                            fs == fsGhost     ? '4' :
                            fs == fsAddedRepo ? '8' :
                            fs == fsNonVcs    ? '9' :
                                                '6'   ];   // For sorting

    if ( fs == fsNonVcs ) {
        pi.FindData.dwFileAttributes |= FILE_ATTRIBUTE_TEMPORARY;
        return;
    }

    if ( fs == fsGhost || fs == fsAddedRepo || fs == fsRemoved )
        pi.FindData.dwFileAttributes |= (entry.bDir ? FILE_ATTRIBUTE_DIRECTORY : 0) | FILE_ATTRIBUTE_HIDDEN;

    if ( IsFileDirty( fs ) )
        pi.FindData.dwFileAttributes |= FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;

    if ( !entry.sRevision.empty() ) {
        pi.CustomColumnData[1] = new char[nRevColumnWidth+1];
        _snprintf( pi.CustomColumnData[1], nRevColumnWidth, "%*s", nRevColumnWidth, fs == fsAdded ? "Added" : entry.sRevision.c_str() );
        pi.CustomColumnData[1][nRevColumnWidth] = 0;
    }

    if ( !entry.sOptions.empty() ) {
        pi.CustomColumnData[2] = new char[nOptColumnWidth+1];
        _snprintf( pi.CustomColumnData[2], nOptColumnWidth, "%-*s", nOptColumnWidth, entry.sOptions.c_str() );
        pi.CustomColumnData[2][nOptColumnWidth] = 0;
    }

    if ( entry.bDir ) {
        if ( !sTag.empty() ) {
            pi.CustomColumnData[3] = new char[sTag.length()+1];
            strcpy( pi.CustomColumnData[3], sTag.c_str() );
        }
    }
    else {
        if ( !entry.sTagdate.empty() ) {
            pi.CustomColumnData[3] = new char[entry.sTagdate.size()+1];
            strcpy( pi.CustomColumnData[3], entry.sTagdate.c_str() + (entry.sTagdate.empty() ? 0 : 1) );
        }
    }
}

//==========================================================================>>
// Prepare the file list for the panel
//==========================================================================>>

int VcsPlugin::GetFindData( PluginPanelItem **ppItems, int *pItemsNumber, int /*OpMode*/ )
{
    // Read the VCS data (does nothing if not in a VCS-controlled directory)

    boost::intrusive_ptr<IVcsData> apVcsData = GetVcsData( szCurDir );

    // Enumerate all the file entries in the current directory

    *ppItems = 0;
    *pItemsNumber = 0;

    PluginPanelItem pi;
    memset( &pi, 0, sizeof PluginPanelItem );

    vector<PluginPanelItem> v; // Let the vector do all the allocation/reallocation work
    v.reserve( 512 );          // It is rare that a directory contains more entries

    if ( apVcsData && apVcsData->IsValid() )
    {
        for ( VcsEntries::iterator p = apVcsData->entries().begin(); p != apVcsData->entries().end(); ++p )
        {
            memset( &pi, 0, sizeof PluginPanelItem );
            pi.FindData = p->second.fileFindData;

            if ( p->second.bDir )
            {
                pi.FindData.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
                string sFullPathName = CatPath( szCurDir, p->first.c_str() );
                
                if ( DirtyDirs.ContainsDown( sFullPathName ) )
                    p->second.status = fsModified;
                else if ( OutdatedFiles.ContainsDown( sFullPathName ) )
                    p->second.status = fsOutdated;
            }

            array_strcpy( pi.FindData.cFileName, strcmp(p->first.c_str(),"..") == 0 ? ".." : CatPath(szCurDir,p->first.c_str()).c_str() );
            DecoratePanelItem( pi, p->second, apVcsData->getTag() );
            v.push_back( pi );
        }
    }
    else
    {
        for ( dir_iterator p = dir_iterator(szCurDir,true); p != dir_iterator(); ++p )
        {
            if ( strcmp( p->cFileName, "." ) == 0 )
                continue;

            memset( &pi, 0, sizeof PluginPanelItem );
            pi.FindData = *p;
            array_strcpy( pi.FindData.cFileName, strcmp(p->cFileName,"..") == 0 ? ".." : CatPath(szCurDir,p->cFileName).c_str() );
            v.push_back( pi );
        }
    }

    // Convert the data into old-fashioned format required by FAR

    if ( v.size() > 0 )
    {
        *ppItems = new PluginPanelItem[v.size()];
        *pItemsNumber = v.size();
        memcpy( *ppItems, &v[0], v.size() * sizeof PluginPanelItem );
    }

    if ( !apVcsData )
        return TRUE;

    // Make sure the non-existing files are not selected

    PanelInfo pinfo;
    StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_GETPANELINFO, &pinfo );

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

    return TRUE;
}

void VcsPlugin::FreeFindData( PluginPanelItem *pItems, int ItemsNumber )
{
    for ( int i = 0; i < ItemsNumber; ++i ) {
        for ( int j = 1; j < pItems[i].CustomColumnNumber; ++j ) // The zeroth item is the status and it is static
            if ( pItems[i].CustomColumnData[j] != cszEmptyLine )
                delete [] pItems[i].CustomColumnData[j];
        delete pItems[i].CustomColumnData;
    }
    delete [] pItems;
}

//==========================================================================>>
// Handle the FE_REDRAW event to keep intact the current cursor position
// when starting the plugin panel
//==========================================================================>>

int VcsPlugin::ProcessEvent( int Event, void * /*Param*/ )
{
    if ( Event == FE_REDRAW )
    {
        if ( *szItemToStart == 0 )
            return FALSE;

        string sItemToStart( szItemToStart );
        *szItemToStart = 0;

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

    return FALSE;
}

//==========================================================================>>
// We have to override the processing of Ctrl+Enter and Ctrl+Insert keys
// because of the full pathnames in our PluginPanelItem structures
//==========================================================================>>

int VcsPlugin::ProcessKey( int Key, unsigned int ControlState )
{
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

        try
        {
            if ( !Executor( cszPluginName,
                            szCurDir,
                            string("cvs annotate ") + ExtractFileName(szCurFile),
                            0,
                            tempFile.GetName().c_str() ).Execute() )
                return FALSE;
        }
        catch( runtime_error& e )
        {
            MsgBoxWarning( cszPluginName, e.what() );
        }

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

//==========================================================================>>
// Directory monitoring thread routine. Starts the VCS plugin panel as soon
// as we enter a VCS-controlled directory
//==========================================================================>>

Thread VcsPlugin::MonitoringThread( VcsPlugin::MonitoringThreadRoutine );

unsigned int VcsPlugin::MonitoringThreadRoutine( void *pStartupInfo, HANDLE hTerminateEvent )
{
    if ( pStartupInfo == 0 )
        return 0;

    static PluginStartupInfo I;
    I = *static_cast<PluginStartupInfo*>( pStartupInfo ); // Note assignment, not initialization

    static vector<unsigned short> CharBuf( nMaxConsoleWidth * nMaxConsoleHeight ); // Character buffer used to scan the console screen

    HANDLE ObjectsToWait[] = { hTerminateEvent }; // WaitForMultipleObjects is used to avoid message processing problems -- see MSDN

    for ( ; ; )
    {
        if ( ::WaitForMultipleObjects( array_size(ObjectsToWait), ObjectsToWait, false, 200 ) != WAIT_TIMEOUT ) // Thread termination requested or something unexpected happened
            return 0; // Exit the thread

        if ( !::Settings.bAutomaticMode )
            continue;

        PanelInfo pi;
        StartupInfo.Control( INVALID_HANDLE_VALUE, FCTL_GETPANELSHORTINFO, &pi );

        if ( pi.PanelType != PTYPE_FILEPANEL || pi.Plugin || !IsVcsDir( pi.CurDir ) )
            continue;

        // Dirty hack to check that no information/menu/dialog is active

        HANDLE hConsoleOutput = ::GetStdHandle( STD_OUTPUT_HANDLE );

        if ( !hConsoleOutput )
            continue;

        CONSOLE_SCREEN_BUFFER_INFO screenBufInfo;
        if ( !::GetConsoleScreenBufferInfo( hConsoleOutput, &screenBufInfo ) )
            continue;

        unsigned long dwScreenBufSize = screenBufInfo.dwSize.X * screenBufInfo.dwSize.Y;
        if ( dwScreenBufSize == 0 || dwScreenBufSize > CharBuf.capacity() )
            continue;

        COORD c = { 0, 0 };
        unsigned long dwRead;
        if ( !::ReadConsoleOutputCharacterW( hConsoleOutput, (LPWSTR)&CharBuf[0], dwScreenBufSize, c, &dwRead ) || dwRead != dwScreenBufSize )
            continue;

        // It only makes sense to start the plugin if there are file panels (at least one) currently displayed.
        // We detect file panels by box drawing characters in the first and the second rows of the screen.

        unsigned short * const cpFirstRowBegin  = &CharBuf[0];
        unsigned short * const cpFirstRowEnd    = cpFirstRowBegin + screenBufInfo.dwSize.X;
        unsigned short * const cpSecondRowBegin = cpFirstRowEnd;
        unsigned short * const cpScreenEnd      = cpFirstRowBegin + dwScreenBufSize;

        unsigned short *pTopLeftCorner1 = *cpFirstRowBegin == 0x2554 || *cpFirstRowBegin == L'[' /*Bg window number*/ ? cpFirstRowBegin : cpFirstRowEnd;
        unsigned short *pTopLeftCorner2 = find( cpFirstRowBegin+1, cpFirstRowEnd, 0x2554 );

        unsigned short *pTopRightCorner1 = find( cpFirstRowBegin+1, pTopLeftCorner2, 0x2557 );
        unsigned short *pTopRightCorner2 = cpFirstRowEnd[-1] == 0x2557 || iswdigit(cpFirstRowEnd[-1]) /*Clock*/ ? cpFirstRowEnd-1 : cpFirstRowEnd;

        bool bFilePanelsDetected =
            pTopLeftCorner1 < cpFirstRowEnd && pTopRightCorner1 < cpFirstRowEnd &&
            cpSecondRowBegin[pTopLeftCorner1-cpFirstRowBegin] == 0x2551 && cpSecondRowBegin[pTopRightCorner1-cpFirstRowBegin] == 0x2551 ||

            pTopLeftCorner2 < cpFirstRowEnd && pTopRightCorner2 < cpFirstRowEnd &&
            cpSecondRowBegin[pTopLeftCorner2-cpFirstRowBegin] == 0x2551 && cpSecondRowBegin[pTopRightCorner2-cpFirstRowBegin] == 0x2551;

        if ( !bFilePanelsDetected )
            continue;

        // A top-left box drawing character anywhere below the first row means a menu or dialog is currently active

        if ( find( cpSecondRowBegin, cpScreenEnd, 0x2554 ) < cpScreenEnd )
            continue;

        // Auto-starting the VCS panel (feasible only if a hotkey for the plugin is defined)

        unsigned char cPluginHotkey = GetPluginHotkey( cszDllName );

        if ( cPluginHotkey == 0 )
            continue;

        HANDLE hConsoleInput = ::GetStdHandle( STD_INPUT_HANDLE );

        if ( !hConsoleInput )
            continue;

        INPUT_RECORD recs[] = { { KEY_EVENT, { TRUE,  1,        VK_F11, 0,             0, 0 } },
                                { KEY_EVENT, { FALSE, 1,        VK_F11, 0,             0, 0 } },
                                { KEY_EVENT, { TRUE,  1, cPluginHotkey, 0, cPluginHotkey, 0 } },
                                { KEY_EVENT, { FALSE, 1, cPluginHotkey, 0, cPluginHotkey, 0 } } };
        DWORD dwWritten;
        ::WriteConsoleInput( hConsoleInput, recs, array_size(recs), &dwWritten );
    }
}

