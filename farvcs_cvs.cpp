/*****************************************************************************
 File name:  cvs.cpp
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
#include "cvsentries.h"

using namespace std;
using namespace boost;

//==========================================================================>>
// Detects whether a directory is CVS-controlled
//==========================================================================>>

bool IsCvsDir( const string& sDir )
{
    return ::GetFileAttributes( CatPath( sDir.c_str(), "CVS\\Entries" ).c_str() ) != (DWORD)-1;
}

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

class CvsData : public VcsData, private noncopyable
{
public:
    explicit CvsData( const std::string& sDir, TSFileSet& DirtyDirs, const TSFileSet& OutdatedFiles ) :
        m_bValid( false ),
        m_sDir( sDir ),
        m_cTagType( 0 ),
        m_bEntriesLoaded( false ),
        m_DirtyDirs( DirtyDirs ),
        m_OutdatedFiles( OutdatedFiles )
    {
        if ( !IsCvsDir( sDir ) )
            return;
        m_bValid = true;
        ReadTagFile( sDir, m_cTagType, m_sTag );
    }

    const VcsEntries& entries() const { return LazyLoadEntries(); }
    VcsEntries& entries()             { return LazyLoadEntries(); }
    const char *getTag() const { return m_sTag.c_str(); }
    const char *getDir() const { return m_sDir.c_str(); }

    bool IsValid() const { return m_bValid; }
    void self_destroy() { delete this; }

private:
    bool m_bValid;

    std::string m_sDir;     // The CVS-controlled directory
    char        m_cTagType; // First character of the 'Tag' file
    std::string m_sTag;     // First line of the 'Tag' file without the first character

    TSFileSet& m_DirtyDirs;
    const TSFileSet& m_OutdatedFiles;

    mutable VcsEntries m_Entries;  // Merged 'Entries' and 'Entries.Log' files
    mutable bool m_bEntriesLoaded;

    VcsEntries& LazyLoadEntries() const
    {
        if ( m_bValid && !m_bEntriesLoaded )  // ToDo: Add synchronization to eliminate potential thread-safety issue
        {
            ReadEntriesFile( false );
            ReadEntriesFile( true );
            FillStatuses();
            m_bEntriesLoaded = true;
        }

        return m_Entries;
    }

    void FillStatuses() const;

    bool ReadEntriesFile( bool bEntriesLog ) const;
    bool ReadTagFile( const std::string& sDir, char& cTagType, std::string& sTag );
};

//==========================================================================>>
// Reads 'Entries' or 'Entries.Log' file
//==========================================================================>>

bool CvsData::ReadEntriesFile( bool bEntriesLog ) const
{
    // Construct the file name

    const char *szEntriesFile = bEntriesLog ? "CVS\\Entries.Log" : "CVS\\Entries";

    ifstream fEntries( CatPath(m_sDir.c_str(),szEntriesFile).c_str() );

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
// Fill in the file statuses
//==========================================================================>>

void CvsData::FillStatuses() const
{
    bool bDirtyFilesExist = false;

    for ( dir_iterator p(m_sDir,true); p != dir_iterator(); ++p )
    {
        VcsEntries::iterator pEntry = m_Entries.find( p->cFileName );
        
        if ( pEntry == m_Entries.end() )
        {
            if ( strcmp( p->cFileName, "." ) == 0 )
                continue;

            if ( strcmp( p->cFileName, "CVS" ) != 0 )
                m_Entries.insert( make_pair( p->cFileName, VcsEntry( p->cFileName, *p ) ) );
            else
                m_Entries.insert( make_pair( p->cFileName, VcsEntry( p->cFileName, *p, fsNormal ) ) );

            continue;
        }

        VcsEntry& entry = pEntry->second;
        string sFullPathName = CatPath( m_sDir.c_str(), p->cFileName );

        assert( entry.status == fsGhost );

        entry.fileFindData = *p;

        entry.status = strcmp( entry.sRevision.c_str(), "0" ) == 0            ? fsAdded    :
                       entry.sRevision.c_str()[0] == '-'                      ? fsRemoved  :
                       entry.sTimestamp.find_first_of( "+" ) != string::npos  ? fsConflict :
                       IsFileModified( p->ftLastWriteTime, entry.sTimestamp ) ? fsModified :
                       m_OutdatedFiles.ContainsEntry( sFullPathName )         ? fsOutdated :
                                                                                fsNormal;
        bDirtyFilesExist |= IsFileDirty( entry.status );
    }

    // Add as "added in repository" the files/directories that are in outdated files but not existing locally
    // and not mentioned by CVS

    for ( TSFileSet::const_iterator p = m_OutdatedFiles.begin(); p != m_OutdatedFiles.end(); ++p )
    {
        string sFileName = ExtractFileName( *p );

        if ( IsFileFromDir()(*p,m_sDir.c_str()) && m_Entries.find(sFileName) == m_Entries.end() )
            m_Entries.insert( make_pair( sFileName, VcsEntry(false,sFileName,"","","",m_sTag,fsAddedRepo) ) );
    }

    // Add/remove the current directory in the list of the directories containing dirty files

    if ( bDirtyFilesExist )
        m_DirtyDirs.Add( m_sDir.c_str() );
    else
        m_DirtyDirs.Remove( m_sDir.c_str() );
}

//==========================================================================>>
// Reads 'Tag' file
//==========================================================================>>

bool CvsData::ReadTagFile( const std::string& sDir, char& cTagType, std::string& sTag )
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

extern "C" __declspec(dllexport) bool IsPluginDir( const std::string& sDir )
{
    return IsCvsDir( sDir );
}

extern "C" __declspec(dllexport) VcsData *GetPluginDirData( const string& sDir, TSFileSet& DirtyDirs, const TSFileSet& OutdatedFiles )
{
    return new CvsData( sDir, DirtyDirs, OutdatedFiles );
}
