#ifndef __VCSENTRIES_H
#define __VCSENTRIES_H

#include <string>
#include <vector>
#include <map>
#include "miscutil.h"

enum EVcsStatus { fsAdded, fsRemoved, fsModified, fsConflict, fsNormal, fsOutdated, fsAddedRepo, fsGhost, fsNonCvs, fsBogus };

bool IsCvsDir( const std::string& sDir );
bool IsCvsDir( const std::string& sParentDir, const WIN32_FIND_DATA& FindData );

//==========================================================================>>
// Encapsulates a single record of the 'Entries' file
//==========================================================================>>

struct VcsEntry
{
    VcsEntry( bool _bDir, const std::string& _sName, const std::string& _sRevision, const std::string& _sTimestamp, const std::string& _sOptions, const std::string& _sTagdate, EVcsStatus vcsStatus ) :
        bDir      ( _bDir ),
        sName     ( _sName ),
        sRevision ( _sRevision ),
        sTimestamp( _sTimestamp ),
        sOptions  ( _sOptions ),
        sTagdate  ( _sTagdate ),
        status( vcsStatus )
    {
        ::memset( &fileFindData, 0, sizeof fileFindData );
    }

    VcsEntry( bool _bDir, const std::vector<std::string>& v, EVcsStatus vcsStatus ) :
        bDir( _bDir ),
        sName     ( v.size() > 0 ? v[0] : "" ),
        sRevision ( v.size() > 1 ? v[1] : "" ),
        sTimestamp( v.size() > 2 ? v[2] : "" ),
        sOptions  ( v.size() > 3 ? v[3] : "" ),
        sTagdate  ( v.size() > 4 ? v[4] : "" ),
        status( vcsStatus )
    {
        ::memset( &fileFindData, 0, sizeof fileFindData );
    }

    VcsEntry() :
        bDir( false ),
        status( fsBogus )
    {
        ::memset( &fileFindData, 0, sizeof fileFindData );
    }

    VcsEntry( const char *szFileName, const WIN32_FIND_DATA& _fileFindData ) :
        bDir( (_fileFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ),
        sName( szFileName ),
        status( fsNonCvs ),
        fileFindData( _fileFindData )
    {}

    bool bDir;
    std::string sName;
    std::string sRevision;
    std::string sTimestamp;
    std::string sOptions;
    std::string sTagdate;
    EVcsStatus status;
    WIN32_FIND_DATA fileFindData;
};

//==========================================================================>>
// Encapsulates the data read from the CVS directory
//==========================================================================>>

class VcsData
{
public:
    typedef std::map<std::string,VcsEntry,LessNoCase> VcsEntries;

public:
    virtual const VcsEntries& entries() const = 0;
    virtual VcsEntries& entries() = 0;
    virtual bool IsValid() const = 0;
    virtual std::string getTag() const = 0;
    virtual std::string getDir() const = 0;
};

class VcsFactory
{
public:
    virtual bool IsVcsDir( const std::string& sDir ) = 0;
    virtual std::auto_ptr<VcsData> GetVcsData( const std::string& sDir ) = 0;
};

std::auto_ptr<VcsData> GetVcsData( const std::string& sDir );

class CvsData : public VcsData
{
public:
    explicit CvsData( const std::string& sDir ) : m_bValid(false), m_sDir(sDir), m_cTagType(0), m_bEntriesLoaded(false)
    {
        if ( !IsCvsDir( sDir ) )
            return;
        m_bValid = true;
        ReadTagFile( sDir, m_cTagType, m_sTag );
    }

    const VcsEntries& entries() const { return LazyLoadEntries(); }
    VcsEntries& entries()             { return LazyLoadEntries(); }
    std::string getTag() const { return m_sTag; }
    std::string getDir() const { return m_sDir; }

    bool IsValid() const { return m_bValid; }

private:
    bool m_bValid;

    std::string m_sDir;     // The CVS-controlled directory
    char        m_cTagType; // First character of the 'Tag' file
    std::string m_sTag;     // First line of the 'Tag' file without the first character

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

class CvsFactory : public VcsFactory
{
public:
    bool IsVcsDir( const std::string& sDir ) const { return IsCvsDir( sDir ); }
    std::auto_ptr<VcsData> GetVcsData( const std::string& sDir ) { return std::auto_ptr<VcsData>( new CvsData( sDir ) ); }
};

extern TSFileSet OutdatedFiles;
extern TSFileSet DirtyDirs;

bool IsFileModified( const FILETIME &ftLastWriteTime, std::string sCvsTimestamp );

inline bool IsFileDirty( EVcsStatus fs )
{
    return fs == fsModified ||
           fs == fsAdded    ||
           fs == fsRemoved  ||
           fs == fsConflict;
}

inline bool IsFileDirty( const WIN32_FIND_DATA& findData, const VcsData& vcsData )
{
    VcsData::VcsEntries::const_iterator p = vcsData.entries().find( ExtractFileName(findData.cFileName).c_str() );
    return p != vcsData.entries().end() ? IsFileDirty(p->second.status) : false;
}

inline bool IsCvsFile( EVcsStatus fs )
{
    return fs != fsBogus &&
           fs != fsNonCvs;
}

inline bool IsCvsFile( const WIN32_FIND_DATA& findData, const VcsData& vcsData )
{
    VcsData::VcsEntries::const_iterator p = vcsData.entries().find( ExtractFileName(findData.cFileName).c_str() );
    return p != vcsData.entries().end() ? IsCvsFile(p->second.status) : false;
}

int CountCvsDirs( const std::string& sCurDir, int nDownToLevel );

#endif // __VCSENTRIES_H
