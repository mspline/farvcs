/*****************************************************************************
 File name:  cvsentries.cpp
 Project:    FarVCS plugin
 Purpose:    Handling of the contents of the CVS administrative directory
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhaus
 Dependencies: STL
*****************************************************************************/

#include <algorithm>
#include <fstream>
#include <time.h>
#include "cvsentries.h"
#include "svn.h"

using namespace std;

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
            if ( strcmp( p->cFileName, "." ) != 0 )
                m_Entries.insert( make_pair( p->cFileName, VcsEntry( p->cFileName, *p ) ) );
            
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
                       OutdatedFiles.ContainsEntry( sFullPathName )           ? fsOutdated :
                                                                                fsNormal;
        bDirtyFilesExist |= IsFileDirty( entry.status );
    }

    // Add as "added in repository" the files/directories that are in outdated files but not existing locally
    // and not mentioned by CVS

    for ( TSFileSet::iterator p = OutdatedFiles.begin(); p != OutdatedFiles.end(); ++p )
    {
        string sFileName = ExtractFileName( *p );

        if ( IsFileFromDir()(*p,m_sDir.c_str()) && m_Entries.find(sFileName) == m_Entries.end() )
            m_Entries.insert( make_pair( sFileName, VcsEntry(false,sFileName,"","","",m_sTag,fsAddedRepo) ) );
    }

    // Add/remove the current directory in the list of the directories containing dirty files

    if ( bDirtyFilesExist )
        DirtyDirs.Add( m_sDir.c_str() );
    else
        DirtyDirs.Remove( m_sDir.c_str() );
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

//==========================================================================>>
// Detects whether a directory is CVS-controlled
//==========================================================================>>

bool IsCvsDir( const string& sDir )
{
    return ::GetFileAttributes( CatPath( sDir.c_str(), "CVS\\Entries" ).c_str() ) != (DWORD)-1;
}

bool IsCvsDir( const string& sParentDir, const WIN32_FIND_DATA& FindData )
{
    return (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
           strcmp( FindData.cFileName, "." ) != 0 && strcmp( FindData.cFileName, ".." ) != 0 &&
           IsCvsDir( CatPath( sParentDir.c_str(), FindData.cFileName ) );
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
// Count the number of CVS directories down to a given level. Level 0 means
// only the directory itself (returns 0 or 1)
//==========================================================================>>

int CountCvsDirs( const string& sCurDir, int nDownToLevel )
{
    if ( !IsCvsDir(sCurDir) )
        return 0;

    int s = 1;

    if ( nDownToLevel > 0 )
        for ( dir_iterator p(sCurDir); p != dir_iterator(); ++p )
            if ( p->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
                s += CountCvsDirs( CatPath(sCurDir.c_str(),p->cFileName), nDownToLevel-1 );

    return s;
}

auto_ptr<VcsData> GetVcsData( const string& sDir )
{
    auto_ptr<VcsData> apVcsData = auto_ptr<VcsData>( new CvsData( sDir ) );
    
    if ( apVcsData->IsValid() )
        return apVcsData;

    apVcsData = auto_ptr<VcsData>( new SvnData( sDir ) );
    return apVcsData;
}
