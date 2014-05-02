#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory.h>
#include <windows.h>
#include "tsset.h"

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
    VcsEntry(bool _bDir, const tstring& _sName, const tstring& _sRevision, const tstring& _sTimestamp, const tstring& _sOptions, const tstring& _sTagdate, EVcsStatus vcsStatus ) :
        bDir      (_bDir),
        sName     (_sName),
        sRevision (_sRevision),
        sTimestamp(_sTimestamp),
        sOptions  (_sOptions),
        sTagdate  (_sTagdate),
        status    (vcsStatus)
    {
        ::memset(&fileFindData, 0, sizeof fileFindData);
    }

    VcsEntry(bool _bDir, const std::vector<tstring>& v, EVcsStatus vcsStatus) :
        bDir      (_bDir),
        sName     (v.size() > 0 ? v[0] : _T("")),
        sRevision (v.size() > 1 ? v[1] : _T("")),
        sTimestamp(v.size() > 2 ? v[2] : _T("")),
        sOptions  (v.size() > 3 ? v[3] : _T("")),
        sTagdate  (v.size() > 4 ? v[4] : _T("")),
        status    (vcsStatus)
    {
        ::memset(&fileFindData, 0, sizeof fileFindData);
    }

    VcsEntry() :
        bDir( false ),
        status( fsBogus )
    {
        ::memset(&fileFindData, 0, sizeof fileFindData);
    }

    VcsEntry(const TCHAR *szFileName, const WIN32_FIND_DATA& _fileFindData, EVcsStatus vcsStatus = fsNonVcs) :
        bDir((_fileFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0),
        sName(szFileName),
        status(vcsStatus),
        fileFindData(_fileFindData)
    {}

    bool bDir;
    tstring sName;
    tstring sRevision;
    tstring sTimestamp;
    tstring sOptions;
    tstring sTagdate;
    EVcsStatus status;
    WIN32_FIND_DATA fileFindData;
};

//==========================================================================>>
// Encapsulates the information in a VCS directory
//==========================================================================>>

typedef std::map<tstring, VcsEntry, LessNoCase> VcsEntries;

struct IVcsData : public ref_countable<IVcsData>
{
    virtual const VcsEntries& entries() const = 0;
    virtual VcsEntries& entries() = 0;
    virtual bool IsValid() const = 0;
    virtual const TCHAR *getTag() const = 0;
    virtual const TCHAR *getDir() const = 0;

    // This pair of methods is used instead of virtual destructor.
    // Indirection is necessary because a descendant can reside is
    // a dll with incompatible runtime.

    void ref_countable_self_destroy() { self_destroy(); } // Statically polymorphic, called from ref_countable
    virtual void self_destroy() = 0;                      // Dynamically polymorphic, overridden in descendants

    virtual bool UpdateStatus(bool bLocal) = 0;
    virtual bool Update(bool bLocal) = 0;
    virtual bool Annotate(const tstring& sFileName, const tstring& sTempFile) = 0;
    virtual bool GetRevisionTemp(const tstring& sFileName, const tstring& sRevision, const tstring& sTempFile) = 0;
    virtual bool Status(const tstring& sFileName, tstring& sWorkingRevision) = 0;
};

bool IsVcsDir(const tstring& sDir);
boost::intrusive_ptr<IVcsData> GetVcsData(const tstring& sDir);

int CountVcsDirs(const tstring& sCurDir, int nDownToLevel);

inline bool IsFileDirty(EVcsStatus fs)
{
    return fs == fsModified ||
           fs == fsAdded    ||
           fs == fsRemoved  ||
           fs == fsConflict;
}

inline bool IsFileDirty(const WIN32_FIND_DATA& findData, const IVcsData& vcsData)
{
    VcsEntries::const_iterator p = vcsData.entries().find(ExtractFileName(findData.cFileName).c_str());
    return p != vcsData.entries().end() && IsFileDirty(p->second.status);
}

inline bool IsVcsFile(EVcsStatus fs)
{
    return fs != fsBogus &&
           fs != fsNonVcs &&
           fs != fsIgnored;
}

inline bool IsVcsFile(const WIN32_FIND_DATA& findData, const IVcsData& vcsData)
{
    VcsEntries::const_iterator p = vcsData.entries().find( ExtractFileName(findData.cFileName).c_str() );
    return p != vcsData.entries().end() && IsVcsFile(p->second.status);
}
