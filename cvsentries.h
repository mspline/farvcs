#ifndef __CVSENTRIES_H
#define __CVSENTRIES_H

#include <string>
#include <vector>
#include <map>
#include "miscutil.h"

bool IsCvsDir( const std::string& sDir );
bool IsCvsDir( const std::string& sParentDir, const WIN32_FIND_DATA& FindData );

//==========================================================================>>
// Encapsulates a single record of the 'Entries' file
//==========================================================================>>

struct CvsEntry
{
    CvsEntry( bool _bDir, const std::string& _sName, const std::string& _sRevision, const std::string& _sTimestamp, const std::string& _sOptions, const std::string& _sTagdate ) :
        bDir(_bDir), sName(_sName), sRevision(_sRevision), sTimestamp(_sTimestamp), sOptions(_sOptions), sTagdate(_sTagdate) {}

    explicit CvsEntry( bool _bDir, const std::vector<std::string>& v ) :
        bDir( _bDir ),
        sName(      v.size() > 0 ? v[0] : "" ),
        sRevision(  v.size() > 1 ? v[1] : "" ),
        sTimestamp( v.size() > 2 ? v[2] : "" ),
        sOptions(   v.size() > 3 ? v[3] : "" ),
        sTagdate(   v.size() > 4 ? v[4] : "" )
    {}

    bool bDir;
    std::string sName;
    std::string sRevision;
    std::string sTimestamp;
    std::string sOptions;
    std::string sTagdate;
};

//==========================================================================>>
// Encapsulates the data read from the CVS directory
//==========================================================================>>

class CvsData
{
public:
    explicit CvsData( const std::string& sDir ) : m_bValid(false), m_sDir(sDir), m_cTagType(0), m_bEntriesLoaded(false)
    {
        if ( !IsCvsDir( sDir ) )
            return;
        m_bValid = true;
        ReadTagFile( sDir, m_cTagType, m_sTag );
    }

    typedef std::map<std::string,CvsEntry,LessNoCase> CvsEntries;

    std::string m_sDir;     // The CVS-controlled directory
    char        m_cTagType; // First character of the 'Tag' file
    std::string m_sTag;     // First line of the 'Tag' file without the first character

    bool operator!() const { return !IsValid(); }
    bool IsValid() const { return m_bValid; }

    const CvsEntries& Entries() const { return LazyLoadEntries(); }
    CvsEntries& Entries()             { return LazyLoadEntries(); }

private:
    bool m_bValid;

    mutable CvsEntries m_Entries;  // Merged 'Entries' and 'Entries.Log' files
    mutable bool m_bEntriesLoaded;

    CvsEntries& LazyLoadEntries() const
    {
        if ( m_bValid && !m_bEntriesLoaded )  // ToDo: Add synchronization to eliminate potential thread-safety issue
        {
            ReadEntriesFile( m_sDir, false, m_Entries );
            ReadEntriesFile( m_sDir, true,  m_Entries );
            m_bEntriesLoaded = true;
        }

        return m_Entries;
    }

    static bool ReadEntriesFile( const std::string& sDir, bool bEntriesLog, CvsEntries& cvsEntries );
    static bool ReadTagFile( const std::string& sDir, char& cTagType, std::string& sTag );
};

extern TSFileSet OutdatedFiles;

enum CvsFileStatus { fsAdded, fsRemoved, fsModified, fsConflict, fsNormal, fsOutdated, fsAddedRepo, fsGhost, fsNonCvs, fsBogus };

bool IsFileModified( const FILETIME &ftLastWriteTime, std::string sCvsTimestamp );
CvsFileStatus GetFileStatus( const WIN32_FIND_DATA& findData, const CvsData& cvsData, const char *szCurDir, const CvsEntry **ppCvsEntry = 0 );

inline bool IsFileDirty( CvsFileStatus fs )
{
    return fs == fsModified ||
           fs == fsAdded    ||
           fs == fsRemoved  ||
           fs == fsConflict;
}

inline bool IsFileDirty( const WIN32_FIND_DATA& findData, const CvsData& cvsData, const char *szCurDir, const CvsEntry **ppCvsEntry = 0 )
{
    return IsFileDirty( GetFileStatus( findData, cvsData, szCurDir, ppCvsEntry ) );
}

inline bool IsCvsFile( const WIN32_FIND_DATA& findData, const CvsData& cvsData, const char *szCurDir, const CvsEntry **ppCvsEntry = 0 )
{
    CvsFileStatus fs = GetFileStatus( findData, cvsData, szCurDir, ppCvsEntry );
    return fs != fsBogus &&
           fs != fsNonCvs;
}

int CountCvsDirs( const std::string& sCurDir, int nDownToLevel );

#endif // __CVSENTRIES_H
