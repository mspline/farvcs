/*****************************************************************************
 File name:  farvcs_svn.cpp
 Project:    FarVCS plugin
 Purpose:    Subversion integration support
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhaus
 Dependencies: STL
*****************************************************************************/

#include <boost/utility.hpp>
#include "vcsdata.h"
#include "plugutil.h"

#undef _CRT_SECURE_NO_DEPRECATE

#include "svn_client.h"
#include "svn_cmdline.h"
#include "svn_pools.h"
#include "svn_config.h"
#include "svn_fs.h"

using namespace std;
using namespace boost;

PluginStartupInfo StartupInfo;
FarStandardFunctions FSF;
HINSTANCE hResInst;

string sPluginName;

class SvnData : public VcsData<SvnData>
{
public:
    explicit SvnData( const string& sDir, TSFileSet& DirtyDirs, TSFileSet& OutdatedFiles ) :
        VcsData( sDir, DirtyDirs, OutdatedFiles )
    {}

    static const char *GetAdminDirName() { return ".svn"; }
    static const bool IsVcsDir( const string& sDir ) { return ::GetFileAttributes( CatPath( sDir.c_str(), ".svn\\entries" ).c_str() ) != (DWORD)-1; }
    
    void self_destroy() { delete this; }

    bool UpdateStatus( bool bLocal );
    bool Update( bool ) { return false; }
    bool Annotate( const string& /*sFileName*/, const string& /*sTempFile*/ ) { return false; }

    // Public Morozov pattern below :)

    using VcsData<SvnData>::m_Entries;
    using VcsData<SvnData>::m_OutdatedFiles;

protected:
    void GetVcsEntriesOnly() const;
};

#define ENF( f ) { if ( f != 0 ) { printf( "ERROR in " #f ); return; } }

class SvnClient : private noncopyable
{
public:
    SvnClient() : m_pool(0), m_ctx(0)
    {
        ENF( svn_cmdline_init( "farvcs", stderr ) );

        m_pool = svn_pool_create(0);

        ENF( svn_fs_initialize( m_pool ) );
        ENF( svn_config_ensure( 0, m_pool ) );
        ENF( svn_client_create_context( &m_ctx, m_pool ) );
        ENF( svn_config_get_config( &m_ctx->config, 0, m_pool ) );

        if ( getenv( "SVN_ASP_DOT_NET_HACK" ) )
            ENF( svn_wc_set_adm_dir( "_svn", m_pool ) );
    }

    virtual ~SvnClient()
    {
        apr_pool_destroy( pool() );
        apr_terminate();
    }

    apr_pool_t       *pool() { return m_pool; }
    svn_client_ctx_t *ctx() { return m_ctx; }

private:
    apr_pool_t *m_pool;
    svn_client_ctx_t *m_ctx;
};

struct StatusCbData
{
    const char *szDir;
    const SvnData *pSvnData;
    bool bUpdateStatus;
};

void svn_wc_status_callback( void *status_baton, const char *path, svn_wc_status2_t *status )
{
    StatusCbData *pcb = reinterpret_cast<StatusCbData *>( status_baton );

    if ( _stricmp( path, pcb->szDir ) == 0 )
        return;

    int dnlen = ::strlen( pcb->szDir );

    assert( _strnicmp( path, pcb->szDir, dnlen ) == 0 );

    const char *szFileName = path + dnlen + 1;

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
    if ( !pcb->bUpdateStatus )
        pcb->pSvnData->m_Entries.insert( make_pair( szFileName,
                                                    VcsEntry( false, 
                                                              szFileName,
                                                              status->entry ? l2s(status->entry->revision) : "",
                                                              "",
                                                              "",
                                                              "",
                                                              fs ) ) );
    else
        if ( fs == fsOutdated )
            pcb->pSvnData->m_OutdatedFiles.Add( path );
}

bool CheckSuccess( svn_error_t *perr, const char *szUserFriendlyMessage )
{
    if ( !perr )
        return true;

    string sError( szUserFriendlyMessage );
    
    for ( ; perr; perr = perr->child )
        sError += sformat( "\n\x01\n[Error %d] %s", perr->apr_err, perr->message );

    MsgBoxWarning( sPluginName.c_str(), sError.c_str() );
    return false;
}

void SvnData::GetVcsEntriesOnly() const
{
    SvnClient svn;

    svn_revnum_t result_rev;
    svn_opt_revision_t requested_rev = { svn_opt_revision_base };

    m_Entries.clear();

    StatusCbData cbdata = { getDir(), this, false };

    svn_error_t *perr = svn_client_status2
    (
        &result_rev,
        getDir(),
        &requested_rev,
        svn_wc_status_callback,
        (void*)&cbdata,
        false, // recurse
        true,  // get_all
        false, // update,
        true,  // no_ignore
        true,  // ignore_externals
        svn.ctx(),
        svn.pool()
    );

    CheckSuccess( perr, "Getting directory entries failed" );
}

bool SvnData::UpdateStatus( bool bLocal )
{
    SvnClient svn;

    svn_revnum_t result_rev;
    svn_opt_revision_t requested_rev = { svn_opt_revision_base };

    StatusCbData cbdata = { getDir(), this, true };

    svn_error_t *perr = svn_client_status2
    (
        &result_rev,
        getDir(),
        &requested_rev,
        svn_wc_status_callback,
        (void*)&cbdata,
        !bLocal, // recurse
        false,   // get_all
        true,    // update,
        true,    // no_ignore
        true,    // ignore_externals
        svn.ctx(),
        svn.pool()
    );

    return CheckSuccess( perr, "Status update failed" );
}

extern "C" __declspec(dllexport) void Initialize( PluginStartupInfo& startupInfo, const char *szPluginName, HINSTANCE hHostInst )
{
    ::StartupInfo = startupInfo;
    FSF = *startupInfo.FSF;
    ::StartupInfo.FSF = &FSF;
    hResInst = hHostInst;

    sPluginName = string(szPluginName) + "/Subversion";
}

extern "C" __declspec(dllexport) bool IsPluginDir( const string& sDir )
{
    return SvnData::IsVcsDir( sDir );
}

extern "C" __declspec(dllexport) IVcsData *GetPluginDirData( const string& sDir, TSFileSet& DirtyDirs, TSFileSet& OutdatedFiles )
{
    return new SvnData( sDir, DirtyDirs, OutdatedFiles );
}
