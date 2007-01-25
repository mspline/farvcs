/*****************************************************************************
 File name:  vcs.cpp
 Project:    FarVCS plugin
 Purpose:    Common VCS utilities
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhaus
 Dependencies: STL
*****************************************************************************/

#include "vcs.h"
#include "plugutil.h"

using namespace std;
using namespace boost;

//==========================================================================>>
// Count the number of VCS directories down to a given level. Level 0 means
// only the directory itself (i.e. returns 1 if the directory is
// VCS-controlled or 0 otherwise)
//==========================================================================>>

int CountVcsDirs( const string& sCurDir, int nDownToLevel )
{
    if ( !IsVcsDir(sCurDir) )
        return 0;

    int s = 1;

    if ( nDownToLevel > 0 )
        for ( dir_iterator p(sCurDir); p != dir_iterator(); ++p )
            if ( p->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
                s += CountVcsDirs( CatPath(sCurDir.c_str(),p->cFileName), nDownToLevel-1 );

    return s;
}

//==========================================================================>>
// Support for second level plugins
//==========================================================================>>

class PluginDll : private noncopyable
{
public:
    PluginDll( const char *szDllName )
    {
        string sDllPathName = CatPath( ExtractPath(GetModuleFileName(hInstance)).c_str(), szDllName );
        m_hModule = ::LoadLibrary( sDllPathName.c_str() );
        
        if ( m_hModule == 0 )
            return;

        Initialize       = (void (*)( PluginStartupInfo& StartupInfo, const char *cszPluginName, HINSTANCE hHostInst ))::GetProcAddress( m_hModule, "Initialize" );
        Uninitialize     = (void (*)())::GetProcAddress( m_hModule, "Uninitialize" );
        IsPluginDir      = (bool (*)( const string& sDir ))::GetProcAddress( m_hModule, "IsPluginDir" );
        GetPluginDirData = (IVcsData *(*)( const string& sDir,TSFileSet& DirtyDirs, TSFileSet& OutdatedFiles ))::GetProcAddress( m_hModule, "GetPluginDirData" );

        if ( Initialize )
            Initialize( StartupInfo, cszPluginName, hInstance );
    }

    virtual ~PluginDll()
    {
        if ( m_hModule )
        {
            if ( Uninitialize )
                Uninitialize();

            ::FreeLibrary( m_hModule  );
        }
    }

    bool IsValid() { return m_hModule != 0; }

    void (*Initialize)( PluginStartupInfo& StartupInfo, const char *cszPluginName, HINSTANCE hHostInst );
    void (*Uninitialize)();
    bool (*IsPluginDir)( const string& sDir );
    IVcsData *(*GetPluginDirData)( const string& sDir, TSFileSet& DirtyDirs, TSFileSet& OutdatedFiles );

private:
    HMODULE m_hModule;
};

// Meyers' singleton

PluginDll *Plugins()
{
    static PluginDll plugins[] =
    {
        PluginDll( "farvcs_cvs.dll" ),
        PluginDll( "farvcs_svn.dll" ),
        PluginDll( "farvcs_p4.dll" )
    };

    return plugins;
};

bool IsVcsDir( const string& sDir )
{
    for ( unsigned int i = 0; i < 3 /* Hack */ ; ++i )
        if ( Plugins()[i].IsValid() && Plugins()[i].IsPluginDir( sDir ) )
            return true;

    return false;
}

boost::intrusive_ptr<IVcsData> GetVcsData( const string& sDir )
{
    for ( unsigned int i = 0; i < 3 /* Hack */ ; ++i )
        if ( Plugins()[i].IsValid() && Plugins()[i].IsPluginDir( sDir ) )
            return Plugins()[i].GetPluginDirData( sDir, DirtyDirs, OutdatedFiles );

    return 0;
}
