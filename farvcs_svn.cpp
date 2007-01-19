/*****************************************************************************
 File name:  farvcs_svn.cpp
 Project:    FarVCS plugin
 Purpose:    Subversion integration support
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhaus
 Dependencies: STL
*****************************************************************************/

#include "cvsentries.h"

#undef _CRT_SECURE_NO_DEPRECATE

#include "svn_client.h"
#include "svn_cmdline.h"
#include "svn_pools.h"
#include "svn_config.h"
#include "svn_fs.h"

using namespace std;
using namespace boost;

bool IsSvnDir( const std::string& sDir )
{
    return ::GetFileAttributes( CatPath( sDir.c_str(), ".svn\\entries" ).c_str() ) != (DWORD)-1;
}

class SvnData : public VcsData, private noncopyable
{
public:
    explicit SvnData( const std::string& sDir, TSFileSet& DirtyDirs, const TSFileSet& OutdatedFiles ) :
        m_bValid( false ),
        m_sDir( sDir ),
        m_bEntriesLoaded( false ),
        m_DirtyDirs( DirtyDirs ),
        m_OutdatedFiles( OutdatedFiles )
    {
        if ( !IsSvnDir( sDir ) )
            return;
        m_bValid = true;
    }

    const VcsEntries& entries() const { return LazyLoadEntries(); }
    VcsEntries& entries()             { return LazyLoadEntries(); }
    const char *getTag() const { return m_sTag.c_str(); }
    const char *getDir() const { return m_sDir.c_str(); }

    bool IsValid() const { return m_bValid; }

    void self_destroy() { delete this; }

private:
    bool m_bValid;

    std::string m_sDir;
    std::string m_sTag;

    TSFileSet& m_DirtyDirs;
    const TSFileSet& m_OutdatedFiles;

    mutable VcsEntries m_Entries;
    mutable bool m_bEntriesLoaded;

    VcsEntries& LazyLoadEntries() const;
    void SvnGetEntries() const;

    void FillStatuses() const;
};

VcsData::VcsEntries& SvnData::LazyLoadEntries() const
{
    if ( m_bValid && !m_bEntriesLoaded )  // ToDo: Add synchronization to eliminate potential thread-safety issue
    {
        SvnGetEntries();
        FillStatuses();
        m_bEntriesLoaded = true;
    }

    return m_Entries;
}

VcsData::VcsEntries g_Entries;
const char *g_szDir;

void svn_wc_status_callback( void *, const char *path, svn_wc_status2_t *status )
{
    string sFileName = ExtractFileName( path );

    if ( _stricmp( g_szDir, path ) == 0 )
        return;

    EVcsStatus fs = status->text_status == svn_wc_status_none         ? fsBogus      :
                    status->text_status == svn_wc_status_unversioned  ? fsNonVcs     :
                    status->text_status == svn_wc_status_normal       ? fsNormal     :
                    status->text_status == svn_wc_status_added        ? fsAdded      :
                    status->text_status == svn_wc_status_missing      ? fsGhost      :
                    status->text_status == svn_wc_status_deleted      ? fsRemoved    :
                    status->text_status == svn_wc_status_replaced     ? fsReplaced   :
                    status->text_status == svn_wc_status_modified     ? fsModified   :
                    status->text_status == svn_wc_status_merged       ? fsMerged     :
                    status->text_status == svn_wc_status_conflicted   ? fsConflict   :
                    status->text_status == svn_wc_status_ignored      ? fsIgnored    :
                    status->text_status == svn_wc_status_obstructed   ? fsObstructed :
                    status->text_status == svn_wc_status_external     ? fsExternal   :
                    status->text_status == svn_wc_status_incomplete   ? fsIncomplete :
                                                                        fsBogus;
    g_Entries.insert( make_pair( sFileName,
                                 VcsEntry( false, 
                                           sFileName,
                                           status->entry ? l2s(status->entry->revision) : "",
                                           "",
                                           "",
                                           "",
                                           fs ) ) );
}

#define ENF( f ) { if ( f != 0 ) { printf( "ERROR in " #f ); return; } }

void SvnData::SvnGetEntries() const
{
    ENF( svn_cmdline_init( "farvcs", stderr ) );

    apr_pool_t *pool = svn_pool_create(0);
    svn_client_ctx_t *ctx;

    ENF( svn_fs_initialize( pool ) );
    ENF( svn_config_ensure( 0, pool ) );
    ENF( svn_client_create_context( &ctx, pool ) );
    ENF( svn_config_get_config( &ctx->config, 0, pool ) );

    if ( getenv( "SVN_ASP_DOT_NET_HACK" ) )
        ENF( svn_wc_set_adm_dir( "_svn", pool ) );

    svn_revnum_t result_rev;
    svn_opt_revision_t requested_rev = { svn_opt_revision_base };

    g_Entries.clear();
    g_szDir = m_sDir.c_str();

    svn_error_t *perr = svn_client_status2
    (
        &result_rev,
        m_sDir.c_str(),
        &requested_rev,
        svn_wc_status_callback,
        0,     // status_baton
        false, // recurse
        true,  // get_all
        false, // update,
        true,  // no_ignore
        true,  // ignore_externals
        ctx,
        pool
    );

    perr;

    m_Entries = g_Entries;
}

void SvnData::FillStatuses() const
{
    bool bDirtyFilesExist = false;

    for ( dir_iterator p(m_sDir,true); p != dir_iterator(); ++p )
    {
        VcsEntries::iterator pEntry = m_Entries.find( p->cFileName );
        
        if ( pEntry == m_Entries.end() )
        {
            if ( strcmp( p->cFileName, "." ) == 0 )
                continue;

            if ( strcmp( p->cFileName, ".svn" ) != 0 )
                m_Entries.insert( make_pair( p->cFileName, VcsEntry( p->cFileName, *p ) ) );
            else
                m_Entries.insert( make_pair( p->cFileName, VcsEntry( p->cFileName, *p, fsNormal ) ) );

            continue;
        }

        VcsEntry& entry = pEntry->second;
        string sFullPathName = CatPath( m_sDir.c_str(), p->cFileName );

        entry.fileFindData = *p;

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

extern "C" __declspec(dllexport) bool IsPluginDir( const std::string& sDir )
{
    return IsSvnDir( sDir );
}

extern "C" __declspec(dllexport) VcsData *GetPluginDirData( const string& sDir, TSFileSet& DirtyDirs, const TSFileSet& OutdatedFiles )
{
    return new SvnData( sDir, DirtyDirs, OutdatedFiles );
}
