#ifndef __TRAVERSE_H
#define __TRAVERSE_H

//--------------------------------------------------------------------------->
// Traverses the file system tree looking for dirty files and filling in
// the DirtyDirs container.
// Returns false if the traverse was cancelled or an error occured.
//--------------------------------------------------------------------------->

#include "longop.h"
#include "lang.h"
#include "vcs.h"

class Traversal : public SimpleLongOperation
{
public:
    Traversal( const char *szPluginName, const char *szDir ) : SimpleLongOperation( szPluginName ),
        m_szDir( szDir )
    {}

protected:
    virtual unsigned long DoGetPrompt() { return M_Traversing; }

    virtual const char *DoGetInitialInfo()
    {
        array_strcpy( m_szTruncatedDir, m_szDir );
        FSF.TruncPathStr( m_szTruncatedDir, W );
        return m_szTruncatedDir;
    }

    virtual bool DoExecute()
    {
        static int cnPreCountedDepth = 2;
        int nPreCountedDirs = CountVcsDirs( m_szDir, cnPreCountedDepth );

        int nCountedDirs = 0; // Tracks the number of the visited directories above a given level

        return Traverse( m_szDir, nPreCountedDirs, cnPreCountedDepth, 0, nCountedDirs );
    }

private:
    bool Traverse( const std::string& sDir, int nPreCountedDirs, int nPreCountedDepth, int nLevel, int& nCountedDirs )
    {
        // Read the VCS data (does nothing if not in a VCS-controlled directory)

        if ( !IsVcsDir( sDir ) )
            return true;

        boost::intrusive_ptr<IVcsData> apVcsData = GetVcsData( sDir );

        // Enumerate all the entries in the current directory

        bool bDirtyFilesExist = false;
        bool bRetValue = true;

        for ( dir_iterator p(sDir); p != dir_iterator(); ++p )
        {
            bDirtyFilesExist |= IsFileDirty( *p, *apVcsData );

            std::string sPathName = CatPath( sDir.c_str(), p->cFileName );

            if ( (p->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsVcsDir(sPathName) )
            {
                if ( nLevel < nPreCountedDepth )
                    ++nCountedDirs;

                if ( Interaction( sPathName.c_str(), nCountedDirs*100/nPreCountedDirs ) && (bRetValue = false, true) )
                    break;

                bRetValue = Traverse( sPathName.c_str(), nPreCountedDirs, nPreCountedDepth, nLevel+1, nCountedDirs );

                if ( !bRetValue )
                    break;
            }
        }

        if ( bDirtyFilesExist )
            DirtyDirs.Add( sDir );
        else
            DirtyDirs.Remove( sDir );

        return bRetValue;
    }

    const char *m_szDir;
    char m_szTruncatedDir[MAX_PATH]; // Used in DoGetInitialInfo method only, but stored here because of lifetime
};

#endif // __TRAVERSE_H
