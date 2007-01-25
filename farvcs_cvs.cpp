/*****************************************************************************
 File name:  farvcs_cvs.cpp
 Project:    FarVCS plugin
 Purpose:    CVS integration for FarVCS plugin
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhaus
 Dependencies: STL
*****************************************************************************/

#include <algorithm>
#include <fstream>
#include <time.h>
#include <boost/utility.hpp>
#include <boost/function.hpp>
#include "longop.h"
#include "vcsdata.h"

using namespace std;
using namespace boost;

PluginStartupInfo StartupInfo;
FarStandardFunctions FSF;
HINSTANCE hResInst;

string sPluginName;

//==========================================================================>>
// Detects whether a file is locally modified
//==========================================================================>>

bool IsFileModified( const FILETIME &ftLastWriteTime, string sCvsTimestamp )
{
    if ( sCvsTimestamp.empty() )
        return false;

    SYSTEMTIME base_st = { 1970, 1, 0, 1, 0, 0, 0, 0 };
    FILETIME base_ft;

    ::SystemTimeToFileTime( &base_st, &base_ft );
    time_t itime = (*(unsigned _int64*)&ftLastWriteTime - *(unsigned _int64*)&base_ft)/10000000L;
    time_t itime_tz = itime + 3600;

    string sModifTime( ::asctime( ::gmtime( &itime ) ) );
    string sModifTimeTz( ::asctime( ::gmtime( &itime_tz ) ) );

    sModifTime.erase( remove( sModifTime.begin(), sModifTime.end(), '\n' ) );
    sModifTimeTz.erase( remove( sModifTimeTz.begin(), sModifTimeTz.end(), '\n' ) );

    size_t i = sCvsTimestamp.find( "  " );
    if ( i != string::npos )
        sCvsTimestamp[i+1] = '0';

    return sCvsTimestamp != sModifTime && sCvsTimestamp != sModifTimeTz;
}

//==========================================================================>>
// Encapsulates the information on a CVS directory
//==========================================================================>>

struct CvsUpProcessor;

class CvsData : public VcsData<CvsData>
{
    friend struct CvsUpProcessor;

public:
    explicit CvsData( const string& sDir, TSFileSet& DirtyDirs, TSFileSet& OutdatedFiles ) :
        VcsData( sDir, DirtyDirs, OutdatedFiles )
    {
        char cTagType;
        string sTag;

        if ( ReadTagFile( sDir, cTagType, sTag ) )
            setTag( sTag.c_str() );
    }

    static const char *GetAdminDirName() { return "CVS"; }
    static const bool IsVcsDir( const string& sDir ) { return ::GetFileAttributes( CatPath( sDir.c_str(), "CVS\\Entries" ).c_str() ) != (DWORD)-1; }
    
    void self_destroy() { delete this; }

    bool UpdateStatus( bool bLocal );
    bool Update( bool bLocal );

protected:
    void GetVcsEntriesOnly() const
    {
        m_Entries.clear();
        ReadEntriesFile( false );
        ReadEntriesFile( true );
    }

    void AdjustVcsEntry( VcsEntry& entry, const WIN32_FIND_DATA& findData ) const
    {
        string sFullPathName = CatPath( getDir(), findData.cFileName );

        entry.status = strcmp( entry.sRevision.c_str(), "0" ) == 0                  ? fsAdded    :
                       entry.sRevision.c_str()[0] == '-'                            ? fsRemoved  :
                       entry.sTimestamp.find_first_of( "+" ) != string::npos        ? fsConflict :
                       IsFileModified( findData.ftLastWriteTime, entry.sTimestamp ) ? fsModified :
                       m_OutdatedFiles.ContainsEntry( sFullPathName )               ? fsOutdated :
                                                                                      fsNormal;
    }

private:
    bool ReadEntriesFile( bool bEntriesLog ) const;
    bool ReadTagFile( const string& sDir, char& cTagType, string& sTag );
};

//==========================================================================>>
// Reads 'Entries' or 'Entries.Log' file
//==========================================================================>>

bool CvsData::ReadEntriesFile( bool bEntriesLog ) const
{
    // Construct the file name

    const char *szEntriesFile = bEntriesLog ? "CVS\\Entries.Log" : "CVS\\Entries";

    ifstream fEntries( CatPath(getDir(),szEntriesFile).c_str() );

    if ( !fEntries )
        return false;

    // Parse the file

    char buf[4096];

    while ( fEntries )
    {
        fEntries.getline( buf, sizeof buf );

        if ( !fEntries )
            break;

        buf[sizeof buf-1] = 0;
        const char *pStartPos = buf;

        bool bRemoveEntry = false;
        bool bDir = false;

        if ( bEntriesLog )
        {
            if ( buf[0] == 'R' && buf[1] == ' ' )
                bRemoveEntry = true;
            else if ( buf[0] == 'A' && buf[1] == ' ' )
                bRemoveEntry = false;
            else
                continue; // Unrecognized format
            pStartPos += 2;
        }

        if ( pStartPos[0] == 'D' )
        {
            bDir = true;
            ++pStartPos;
        }

        if ( pStartPos[0] != '/' ) // Unrecognized format
            continue;

        vector<string> vFields = SplitString( pStartPos+1, '/' );
        if ( vFields.empty() ) // Unrecognized format
            continue;

        if ( !bRemoveEntry )
            m_Entries.insert( make_pair( vFields[0], VcsEntry( bDir, vFields, fsGhost ) ) );
        else
            m_Entries.erase( vFields[0] );
    }

    return true;
}

//==========================================================================>>
// Reads 'Tag' file
//==========================================================================>>

bool CvsData::ReadTagFile( const string& sDir, char& cTagType, string& sTag )
{
    cTagType = 0;
    sTag.clear();

    // Construct the file name

    ifstream fTag( CatPath(sDir.c_str(),"CVS\\Tag").c_str() );

    if ( !fTag )
        return false;

    // Read the first line of the file and ignore the rest

    char buf[4096];

    fTag.getline( buf, sizeof buf );
    buf[sizeof buf/sizeof *buf-1] = 0;

    if ( !fTag )
        return false;

    cTagType = *buf;
    sTag = buf+1;
    return true;
}

struct CvsUpProcessor
{
    CvsUpProcessor( CvsData& cvsData, bool bReal ) :
        m_pCvsData( &cvsData ),
        m_bReal( bReal )
    {}

    void operator()( char *sz )
    {
        if ( ::strlen(sz) > 1 && (sz[0] == 'U' || sz[0] == 'P') && sz[1] == ' ' )
        {
            for ( char *p = sz+2; *p; ++p )
                if ( *p == '/' )
                    *p = '\\';

            if ( m_bReal )
                m_pCvsData->m_OutdatedFiles.Remove( CatPath( m_pCvsData->getDir(), sz+2 ) );
            else
                m_pCvsData->m_OutdatedFiles.Add( CatPath( m_pCvsData->getDir(), sz+2 ) );
        }
    }

    CvsData *m_pCvsData;
    bool m_bReal;
};

std::string GetGlobalFlags()
{
    return sformat( " -z%d", 9 /* !!! m_VcsPlugin.Settings.nCompressionLevel*/ );
}

bool CvsData::UpdateStatus( bool bLocal )
{
    string sGlobalFlags = GetGlobalFlags() + " -n";
    string sCommandFlags = bLocal ? " -l" : "";

    string sCmdLine = sformat( "cvs%s up%s", sGlobalFlags.c_str(), sCommandFlags.c_str() );

    return Executor( sPluginName.c_str(), getDir(), sCmdLine, &function<void(char*)>(CvsUpProcessor(*this,false)) ).Execute();
}

bool CvsData::Update( bool bLocal )
{
    string sGlobalFlags = GetGlobalFlags();
    string sCommandFlags = bLocal ? " -l" : "";

    string sCmdLine = sformat( "cvs%s up%s", sGlobalFlags.c_str(), sCommandFlags.c_str() );

    return Executor( sPluginName.c_str(), getDir(), sCmdLine, &function<void(char*)>(CvsUpProcessor(*this,true)) ).Execute();
}

//==========================================================================>>
// Second level plugin interface
//==========================================================================>>

extern "C" __declspec(dllexport) void Initialize( PluginStartupInfo& startupInfo, const char *szPluginName, HINSTANCE hHostInst )
{
    ::StartupInfo = startupInfo;
    FSF = *startupInfo.FSF;
    ::StartupInfo.FSF = &FSF;
    hResInst = hHostInst;

    sPluginName = string(szPluginName) + "/CVS";
}

extern "C" __declspec(dllexport) bool IsPluginDir( const string& sDir )
{
    return CvsData::IsVcsDir( sDir );
}

extern "C" __declspec(dllexport) IVcsData *GetPluginDirData( const string& sDir, TSFileSet& DirtyDirs, TSFileSet& OutdatedFiles )
{
    return new CvsData( sDir, DirtyDirs, OutdatedFiles );
}

