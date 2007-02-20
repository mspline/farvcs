/*****************************************************************************
 File name:  vcsdata.h
 Project:    FarVCS plugin
 Purpose:    Base class for the implementation of IVcsData interface in
             the second level plugins
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhaus
 Dependencies: STL
*****************************************************************************/

#ifndef __VCSDATA_H
#define __VCSDATA_H

#include "vcs.h"

//==========================================================================>>
// Reusable implementation for VcsData descendants
//==========================================================================>>

template <typename D> class VcsData : public IVcsData, private boost::noncopyable
{
public:
    explicit VcsData( const std::string& sDir, TSFileSet& DirtyDirs, TSFileSet& OutdatedFiles ) :
        m_bValid( D::IsVcsDir( sDir ) ),
        m_sDir( sDir ),
        m_bEntriesLoaded( false ),
        m_DirtyDirs( DirtyDirs ),
        m_OutdatedFiles( OutdatedFiles )
    {}

    const VcsEntries& entries() const { return LazyLoadEntries(); }
    VcsEntries& entries()             { return LazyLoadEntries(); }

    const char *getTag() const { return m_sTag.c_str(); }
    const char *getDir() const { return m_sDir.c_str(); }

    bool IsValid() const { return m_bValid; }

protected:
    virtual void GetVcsEntriesOnly() const = 0;
    virtual void AdjustVcsEntry( VcsEntry&, const WIN32_FIND_DATA& ) const {}

    mutable VcsEntries m_Entries;

    TSFileSet& m_DirtyDirs;
    TSFileSet& m_OutdatedFiles;

    void setTag( const char *szTag ) { m_sTag = szTag; }

private:
    bool m_bValid;
    mutable bool m_bEntriesLoaded;

    std::string m_sDir;
    std::string m_sTag;

    VcsEntries& LazyLoadEntries() const;
};

template <typename D> VcsEntries& VcsData<D>::LazyLoadEntries() const
{
    // ToDo: Add synchronization to eliminate potential thread-safety issue

    if ( !m_bValid || m_bEntriesLoaded )
        return m_Entries;

    m_bEntriesLoaded = true; // Do that immediately to prevent infinite recursion

    GetVcsEntriesOnly();

    for ( dir_iterator p(m_sDir,true); p != dir_iterator(); ++p )
    {
        VcsEntries::iterator pEntry = m_Entries.find( p->cFileName );
        
        if ( pEntry != m_Entries.end() )
            AdjustVcsEntry( pEntry->second, pEntry->second.fileFindData = *p );
        else
            if ( strcmp( p->cFileName, "." ) != 0 )
                m_Entries.insert( make_pair( p->cFileName,
                                             VcsEntry( p->cFileName, *p, strcmp(p->cFileName,D::GetAdminDirName()) == 0 ? fsNormal : fsNonVcs ) ) );
    }

    // Add as "added in repository" the files/directories that are in outdated files but not existing locally
    // and not mentioned by VCS

    for ( TSFileSet::const_iterator p = m_OutdatedFiles.begin(); p != m_OutdatedFiles.end(); ++p )
    {
        string sFileName = ExtractFileName( *p );

        if ( IsFileFromDir()(*p,m_sDir.c_str()) && m_Entries.find(sFileName) == m_Entries.end() )
            m_Entries.insert( make_pair( sFileName, VcsEntry(false,sFileName,"","","",m_sTag,fsAddedRepo) ) );
    }

    // Add/remove the current directory in the list of the directories containing dirty files

    bool bDirtyFilesExist = false;
    
    for ( VcsEntries::const_iterator pEntry = m_Entries.begin(); pEntry != m_Entries.end() && !bDirtyFilesExist; ++pEntry )
    	bDirtyFilesExist |= IsFileDirty( pEntry->second.status );

    if ( bDirtyFilesExist )
        m_DirtyDirs.Add( m_sDir.c_str() );
    else
        m_DirtyDirs.Remove( m_sDir.c_str() );

    return m_Entries;
}

#endif // __VCSDATA_H
