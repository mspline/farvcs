#ifndef __IVCS_H
#define __IVCS_H

// Interface to be provided by a version-control system's adapter

struct IVCS
{
    void Annotate( const std::string& sName ) = 0;
    void Add( const std::string& sName ) = 0;
    void Add( std::vector<std::string> vsNames ) = 0;
    void Remove( const std::string& sName ) = 0;
    void Remove( std::vector<std::string> vsNames ) = 0;
};

class CvsAdapter : public IVCS
{
    void Annotate( const std::string& sCurDir, const std::string& sName, const std::string& sOutputFile )
    {
        Execute( sCurDir, sformat("cvs annotate %s", sName.c_str()), sOutputFile );
         
    }
};

#endif // __IVCS_H
