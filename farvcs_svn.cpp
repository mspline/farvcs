/*****************************************************************************
 File name:  farvcs_svn.cpp
 Project:    FarVCS plugin
 Purpose:    Subversion integration support
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhaus
 Dependencies: STL
*****************************************************************************/

#include "vcsdata.h"

#undef _CRT_SECURE_NO_DEPRECATE

#include "svn_client.h"
#include "svn_cmdline.h"
#include "svn_pools.h"
#include "svn_config.h"
#include "svn_fs.h"

using namespace std;

class SvnData : public VcsData<SvnData>
{
public:
    explicit SvnData( const string& sDir, TSFileSet& DirtyDirs, TSFileSet& OutdatedFiles ) :
        VcsData( sDir, DirtyDirs, OutdatedFiles )
    {}

    static const char *GetAdminDirName() { return ".svn"; }
    static const bool IsVcsDir( const string& sDir ) { return ::GetFileAttributes( CatPath( sDir.c_str(), ".svn\\entries" ).c_str() ) != (DWORD)-1; }
    
    void self_destroy() { delete this; }

protected:
    void GetVcsEntriesOnly() const;
};

VcsEntries g_Entries;
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

void SvnData::GetVcsEntriesOnly() const
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
    g_szDir = getDir();

    svn_error_t *perr = svn_client_status2
    (
        &result_rev,
        getDir(),
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

    apr_pool_destroy(pool);
}

extern "C" __declspec(dllexport) bool IsPluginDir( const string& sDir )
{
    return SvnData::IsVcsDir( sDir );
}

extern "C" __declspec(dllexport) IVcsData *GetPluginDirData( const string& sDir, TSFileSet& DirtyDirs, TSFileSet& OutdatedFiles )
{
    return new SvnData( sDir, DirtyDirs, OutdatedFiles );
}
