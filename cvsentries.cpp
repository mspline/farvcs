/*****************************************************************************
 File name:  cvsentries.cpp
 Project:    FarVCS plugin
 Purpose:    Handling of the contents of the CVS administrative directory
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhaus
 Dependencies: STL
*****************************************************************************/

#include <algorithm>
#include <fstream>
#include <time.h>
#include "cvsentries.h"

using namespace std;

//==========================================================================>>
// Count the number of CVS directories down to a given level. Level 0 means
// only the directory itself (returns 0 or 1)
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
