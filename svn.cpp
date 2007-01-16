/*****************************************************************************
 File name:  svn.cpp
 Project:    FarVCS plugin
 Purpose:    Subversion integration support
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhaus
 Dependencies: STL
*****************************************************************************/

#include "svn.h"

/*
#undef _CRT_SECURE_NO_DEPRECATE
#include "svn_client.h"
#include "svn_cmdline.h"
#include "svn_pools.h"
#include "svn_config.h"
#include "svn_fs.h"
*/

using namespace std;

bool IsSvnDir( const string& sDir )
{
    return false; // ::GetFileAttributes( CatPath( sDir.c_str(), ".svn\\entries" ).c_str() ) != (DWORD)-1;
}

bool IsSvnDir( const string& sParentDir, const WIN32_FIND_DATA& FindData )
{
    return (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
           strcmp( FindData.cFileName, "." ) != 0 && strcmp( FindData.cFileName, ".." ) != 0 &&
           IsSvnDir( CatPath( sParentDir.c_str(), FindData.cFileName ) );
}

VcsData::VcsEntries& SvnData::LazyLoadEntries() const
{
    if ( m_bValid && !m_bEntriesLoaded )  // ToDo: Add synchronization to eliminate potential thread-safety issue
    {
        SvnGetEntries();
        m_bEntriesLoaded = true;
    }

    return m_Entries;
}

VcsData::VcsEntries g_Entries;
/*
void svn_wc_status_callback( void *, const char *path, svn_wc_status2_t *status )
{
    if ( *status->entry->name == 0 )
        return;

    g_Entries.insert( make_pair( status->entry->name,
                                 VcsEntry( false, 
                                           status->entry->name,
                                           l2s(status->entry->revision),
                                           "",
                                           "",
                                           "",
                                           status->text_status == 8 ? fsModified : fsNormal ) ) );
}
*/
#define ENF( f ) { if ( f != 0 ) { printf( "ERROR in " #f ); return; } }

void SvnData::SvnGetEntries() const
{
/*
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

    svn_error_t *perr = svn_client_status2
    (
        &result_rev,
        "d:/work/farvcs",
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

    m_Entries = g_Entries;
*/
}
