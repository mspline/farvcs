#ifndef __VCS_H
#define __VCS_H

#include <string>
#include <vector>
#include <map>
#include <memory.h>
#include <windows.h>
#include "miscutil.h"

enum EVcsStatus
{
    fsAdded,
    fsRemoved,
    fsModified,
    fsConflict,
    fsNormal,
    fsOutdated,
    fsAddedRepo,
    fsGhost,
    fsNonVcs,
    fsBogus,

    // Subversion
    
    fsReplaced,
    fsMerged,
    fsIgnored,
    fsObstructed,
    fsExternal,
    fsIncomplete
};

extern TSFileSet DirtyDirs;
extern TSFileSet OutdatedFiles;

//==========================================================================>>
// Encapsulates the information on a single entry in a VCS directory
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

    VcsEntry( const char *szFileName, const WIN32_FIND_DATA& _fileFindData, EVcsStatus vcsStatus = fsNonVcs ) :
        bDir( (_fileFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ),
        sName( szFileName ),
        status( vcsStatus ),
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
// Encapsulates the information on a VCS directory
//==========================================================================>>

class VcsData : public ref_countable<VcsData>
{
public:
    typedef std::map<std::string,VcsEntry,LessNoCase> VcsEntries;

public:
    virtual const VcsEntries& entries() const = 0;
    virtual VcsEntries& entries() = 0;
    virtual bool IsValid() const = 0;
    virtual const char *getTag() const = 0;
    virtual const char *getDir() const = 0;

    // This pair of methods is used instead of virtual destructor.
    // Indirection is necessary because a descendant can reside is
    // a dll with incompatible runtime.

    void ref_countable_self_destroy() { self_destroy(); } // Statically polymorphic, called from ref_countable
    virtual void self_destroy() = 0;                      // Dynamically polymorphic, overridden in descendants
};

bool IsVcsDir( const std::string& sDir );
boost::intrusive_ptr<VcsData> GetVcsData( const std::string& sDir );

int CountVcsDirs( const std::string& sCurDir, int nDownToLevel );

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

inline bool IsVcsFile( EVcsStatus fs )
{
    return fs != fsBogus &&
           fs != fsNonVcs;
}

inline bool IsVcsFile( const WIN32_FIND_DATA& findData, const VcsData& vcsData )
{
    VcsData::VcsEntries::const_iterator p = vcsData.entries().find( ExtractFileName(findData.cFileName).c_str() );
    return p != vcsData.entries().end() ? IsVcsFile(p->second.status) : false;
}

#endif // __VCS_H
