#ifndef __SVN_H
#define __SVN_H

#include "cvsentries.h"

bool IsSvnDir( const std::string& sDir );
bool IsSvnDir( const std::string& sParentDir, const WIN32_FIND_DATA& FindData );

class SvnData : public VcsData
{
public:
    explicit SvnData( const std::string& sDir ) : m_bValid(false), m_sDir(sDir), m_bEntriesLoaded(false)
    {
        if ( !IsSvnDir( sDir ) )
            return;
        m_bValid = true;
    }

    const VcsEntries& entries() const { return LazyLoadEntries(); }
    VcsEntries& entries()             { return LazyLoadEntries(); }
    std::string getTag() const { return m_sTag; }
    std::string getDir() const { return m_sDir; }

    bool IsValid() const { return m_bValid; }

private:
    bool m_bValid;

    std::string m_sDir;
    std::string m_sTag;

    mutable VcsEntries m_Entries;
    mutable bool m_bEntriesLoaded;

    VcsEntries& LazyLoadEntries() const;
    void SvnGetEntries() const;
};

#endif // __SVN_H
