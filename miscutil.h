#ifndef __MISCUTIL_H
#define __MISCUTIL_H

/*****************************************************************************
 File name:  miscutil.h
 Project:    FarVCS plugin
 Purpose:    General-purpose utilities
 Compiler:   MS Visual C++ 8.0
 Authors:    Michael Steinhause
 Dependencies: STL, Win32
*****************************************************************************/

#include <string>
#include <vector>
#include <set>
#include <functional>
#include <algorithm>
#include <process.h>
#include <windows.h>
#include <shlobj.h>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/intrusive_ptr.hpp>
#pragma warning(disable:4512)
#pragma warning(disable:4100)
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#pragma warning(default:4512)
#pragma warning(default:4100)
#include <boost/serialization/set.hpp>

//==========================================================================>>
// printf-like wrapper around ::OutputDebugString
//
// Should be in cpp-file, not here, but it is more convenient this way
// as it is used in projects with static and dynamic runtimes
//==========================================================================>>

inline void Log( const char *szFormat, ... )
{
    va_list args;
    char szBuf[2048];
    long len;

    va_start( args, szFormat );
    len = _vsnprintf( szBuf, sizeof(szBuf), szFormat, args );
    va_end(args);

    if ( len < 0 || len > sizeof(szBuf)-1 )
        len = sizeof(szBuf)-1;

    szBuf[len] = '\0';

    ::OutputDebugString( szBuf );
}

//==========================================================================>>
// Trivia
//==========================================================================>>

typedef std::string tstring;

template <typename T, int N> inline unsigned int array_size( const T(&)[N] )
{
    return N;
}

template <int N> inline char *array_strcpy( char (&dst)[N], const char *src )
{
    char *ret = ::strncpy( dst, src, N );
    dst[N-1] = 0;
    return ret;
}

template <int N> inline char *array_strncat( char (&dst)[N], const char *src, size_t count )
{
    char *ret = ::strncat( dst, src, min(count,N-::strlen(dst)-1) );
    dst[N-1] = 0;
    return ret;
}

template <int N> inline int array_sprintf( char (&dst)[N], const char *szFormat, ... )
{
    va_list args;

    va_start( args, szFormat );
    int len = ::_vsnprintf( dst, N, szFormat, args );
    va_end(args);

    dst[N-1] = 0;
    return len;
}

std::vector<std::string> SplitString( const std::string& s, char c );
void SafeSleep( DWORD dwMilliseconds );

inline std::string GetModuleFileName( HMODULE hModule )
{
    char szFileName[MAX_PATH];
    int len = ::GetModuleFileNameA( hModule, szFileName, array_size(szFileName)-1 );
    szFileName[len] = 0;
    return szFileName;
}

inline std::string u2s( unsigned int u )
{
    char buf[256];
    array_sprintf( buf, "%u", u );
    return buf;
}

inline std::string i2s( int i )
{
    char buf[256];
    array_sprintf( buf, "%d", i );
    return buf;
}

inline std::string l2s( long l )
{
    char buf[256];
    array_sprintf( buf, "%ld", l );
    return buf;
}

inline std::string ul2s( unsigned long ul )
{
    char buf[256];
    array_sprintf( buf, "%lu", ul );
    return buf;
}

//==========================================================================>>
// String comparison functors
//==========================================================================>>

struct StartsWithDir : public std::binary_function<std::string, std::string, bool>
{
    result_type operator()( const first_argument_type& str, const second_argument_type& substr ) const
    {
        return str.compare(0,substr.length(),substr) == 0 &&
               (str.length() == substr.length() || strchr( "\\/:", str[substr.length()] ) != 0 );
    }
};

struct IsFileFromDir : public std::binary_function<std::string, std::string, bool>
{
    result_type operator()( const first_argument_type& str, const second_argument_type& substr ) const
    {
        return str.compare(0,substr.length(),substr) == 0 &&
               str.find_last_of( "\\/:" ) == substr.length();
    }
};

struct LessNoCase : public std::binary_function<std::string, std::string, bool>
{
    result_type operator()( const first_argument_type& s1, const second_argument_type& s2 ) const
    {
        return _stricmp( s1.c_str(), s2.c_str() ) < 0;
    }
};

struct EqualNoCase : public std::binary_function<std::string, std::string, bool>
{
    result_type operator()( const first_argument_type& s1, const second_argument_type& s2 ) const
    {
        return _stricmp( s1.c_str(), s2.c_str() ) == 0;
    }
};

//==========================================================================>>
// Critical section wrapper and guard
//==========================================================================>>

class CriticalSection
{
public:
    CriticalSection() { ::InitializeCriticalSection( &cs ); }
    ~CriticalSection() { ::DeleteCriticalSection( &cs ); }

    void Enter() { ::EnterCriticalSection( &cs ); }
    void Leave() { ::LeaveCriticalSection( &cs ); }

private:
    CRITICAL_SECTION cs;

    CriticalSection( const CriticalSection& );
    CriticalSection& operator=( const CriticalSection& );
};

class CSGuard
{
public:
    CSGuard( CriticalSection& _cs ) : cs(_cs) { cs.Enter(); }
    ~CSGuard() { cs.Leave(); }

private:
    CriticalSection& cs;

    CSGuard( const CSGuard& );
    CSGuard& operator=( const CSGuard& );
};

//==========================================================================>>
// Thread-safe set of full file names
//==========================================================================>>

class TSFileSet
{
public:
    typedef std::set<std::string>::iterator iterator;
    typedef std::set<std::string>::const_iterator const_iterator;

    iterator begin() { return cont.begin(); }
    iterator end()   { return cont.end();   }

    const_iterator begin() const { return cont.begin(); }
    const_iterator end()   const { return cont.end();   }

public:
    void Add( const std::string& sFile )                 { CSGuard _(cs); cont.insert(sFile); }
    void Remove( const std::string& sFile )              { CSGuard _(cs); cont.erase(sFile); }
    bool ContainsEntry( const std::string& sFile ) const { CSGuard _(cs); return std::find_if( cont.begin(), cont.end(), std::bind2nd(EqualNoCase(),sFile) ) != cont.end(); }
    bool ContainsDown( const std::string& sDir ) const   { CSGuard _(cs); return std::find_if( cont.begin(), cont.end(), std::bind2nd(StartsWithDir(),sDir) ) != cont.end(); }
    void RemoveFilesOfDir( const std::string& sDir )     { CSGuard _(cs); cont.erase( std::remove_if( cont.begin(), cont.end(), std::bind2nd(IsFileFromDir(),sDir) ), cont.end() ); }
    void RemoveFilesDownDir( const std::string& sDir )   { CSGuard _(cs); cont.erase( std::remove_if( cont.begin(), cont.end(), std::bind2nd(StartsWithDir(),sDir) ), cont.end() ); }
    void Merge( const TSFileSet& rhs )                   { CSGuard _(cs); cont.insert( rhs.cont.begin(), rhs.cont.end() ); }

private:
    std::set<std::string> cont;
    mutable CriticalSection cs;

private:
    friend class boost::serialization::access;
    template <class Archive> void serialize( Archive& ar, const unsigned int ) { CSGuard _(cs); ar & cont; }
};

//==========================================================================>>
// Thread class. Encapsulates thread starting and graceful stopping
//==========================================================================>>

class Thread
{
public:
    typedef unsigned int ThreadRoutine( void *p, HANDLE hTerminateEvent );

    Thread( ThreadRoutine *pThreadRoutine ) : m_pThreadRoutine(pThreadRoutine), m_hTerminateEvent(0), m_hThread(0) {}
    virtual ~Thread() { Stop(); }
    HRESULT Start( void *Param );
    void Stop( unsigned long dwMilliseconds = 1000 );

private:
    Thread( const Thread& );
    Thread& operator=( const Thread& );

    HANDLE m_hThread;
    HANDLE m_hTerminateEvent;
    ThreadRoutine *m_pThreadRoutine;
    void *m_Param;

    static unsigned int __stdcall ThreadWrapper( void *pThread )
    {
        Thread& thread = *static_cast<Thread*>( pThread );
        return thread.m_pThreadRoutine( thread.m_Param, thread.m_hTerminateEvent );
    }
};

//==========================================================================>>
// Constructs new directory name from a current directory name and a
// relative directory name.
// If the relative directory name turns out absolute, it simply replaces
// the current directory name.
// If the relateive directory name is "..", the current directory name
// is shortened to its parent directory name.
//
// Should be in cpp-file, not here, but it is more convenient this way
// as it is used in projects with static and dynamic runtimes
//==========================================================================>>

inline std::string CatPath( const char *szCurDir, const char *szSubDir )
{
    if ( szCurDir == 0 || *szCurDir == 0 )
        return "";
    else if ( szSubDir == 0 || *szSubDir == 0 )
        return szCurDir;
    else if ( szSubDir[1] == ':' )
        return szSubDir;
    else if ( strchr( "\\/", *szSubDir ) != 0 )
        return szCurDir[1] == ':' ? std::string(szCurDir,2) + szSubDir : szSubDir;
    else if ( strcmp( szSubDir, ".." ) == 0 )
    {
        const char *p = szCurDir + strlen(szCurDir) - 1;
        if ( strchr( "\\/", *p ) != 0 && p > szCurDir && *(p-1) != ':' )
            --p;
        while ( p >= szCurDir && strchr( "\\/", *p ) == 0 )
            --p;

        return p < szCurDir                   ? szCurDir :
               p == szCurDir || *(p-1) == ':' ? std::string( szCurDir, p+1-szCurDir ) :
                                                std::string( szCurDir, p-szCurDir );
    }
    else
    {
        std::string sNewDir = szCurDir;
        if ( !sNewDir.empty() && strchr( "\\/:", *sNewDir.rbegin() ) == 0 )
            sNewDir += "\\";
        return sNewDir + szSubDir;
    }
}

//==========================================================================>>
// Extracts the filename and the path from a full pathname
//
// Should be in cpp-file, not here, but it is more convenient this way
// as it is used in projects with static and dynamic runtimes
//==========================================================================>>

inline std::string ExtractFileName( const std::string& sPathName )
{
    size_t iLastSlash = sPathName.find_last_of( "\\/:" );
    return iLastSlash == std::string::npos ? sPathName : sPathName.substr( iLastSlash+1 );
}

inline std::string ExtractPath( const std::string& sPathName )
{
    size_t iLastSlash = sPathName.find_last_of( "\\/:" );
    return iLastSlash == std::string::npos ? "" : sPathName.substr( 0, iLastSlash );
}

//==========================================================================>>
// STL-compliant iterator wrapper around ::FindFirstFile/::FindNextFile
//==========================================================================>>

// ref_countable CRTP base

template <typename T> class ref_countable
{
private:
    friend void intrusive_ptr_add_ref( ref_countable<T> *p )
    {
        ++static_cast<T*>(p)->ref_counter;
    }

    friend void intrusive_ptr_release( ref_countable<T> *p )
    {
        if ( --static_cast<T*>(p)->ref_counter == 0 )
            static_cast<T*>(p)->ref_countable_self_destroy();
    }

protected:
    ref_countable() : ref_counter(0) {}
    void ref_countable_self_destroy() { delete static_cast<T*>(this); }

private:
    int ref_counter;
};

// The directory iterator proper

class dir_iterator : public boost::iterator_facade<dir_iterator,WIN32_FIND_DATA,boost::single_pass_traversal_tag>
{
public:
    dir_iterator() {}
    explicit dir_iterator( const std::string& sDir, bool bWithDots = false ) : paccessor_( new dir_accessor( sDir, bWithDots ) ) {}

private:
    friend class boost::iterator_core_access;

    void increment()
    {
        paccessor_->advance();
    }

    bool equal( const dir_iterator& rhs ) const
    {
        return paccessor_ == rhs.paccessor_ ||
               (!paccessor_ || !paccessor_->is_valid()) && (!rhs.paccessor_ || !rhs.paccessor_->is_valid());
    }

    reference dereference() const
    {
        return paccessor_->findData_;
    }

private:
    struct dir_accessor : public ref_countable<dir_accessor>
    {
        explicit dir_accessor( const std::string& sDir, bool bWithDots ) : bWithDots_(bWithDots)
        {
            hFind_ = ::FindFirstFile( CatPath(sDir.c_str(),"*").c_str(), &findData_ );
            if ( hFind_ == INVALID_HANDLE_VALUE && ::GetLastError() != ERROR_NO_MORE_FILES )
                throw std::runtime_error( "::FindFirstFile failed" );
            if ( !bWithDots_ && is_dot_entry( findData_.cFileName ) )
                advance();
        }

        ~dir_accessor()
        {
            release_handle();
        }

        void advance()
        {
            if ( hFind_ == INVALID_HANDLE_VALUE )
                throw std::runtime_error( "Invalid attempt to advance to the next item in dir_accessor" );

            if ( !::FindNextFile( hFind_, &findData_ ) )
                if ( ::GetLastError() == ERROR_NO_MORE_FILES )
                    release_handle();
                else
                    throw std::runtime_error( "::FindNextFile failed" );

            if ( !bWithDots_ && is_dot_entry( findData_.cFileName ) )
                advance();
        }

        bool is_valid()
        {
            return hFind_ != INVALID_HANDLE_VALUE;
        }

        HANDLE hFind_;
        WIN32_FIND_DATA findData_;
        bool bWithDots_;

    private:
        void release_handle()
        {
            if ( hFind_ != INVALID_HANDLE_VALUE && !::FindClose( hFind_ ) )
                throw std::runtime_error( "::FindClose failed" );

            hFind_ = INVALID_HANDLE_VALUE;
        }

        bool is_dot_entry( const char *szName )
        {
            return strcmp( szName, "." ) == 0 || strcmp( szName, ".." ) == 0;
        }
    };

    boost::intrusive_ptr<dir_accessor> paccessor_;
};

//==========================================================================>>
// GetTempPath/GetTempFileName wrappers
//==========================================================================>>

std::string GetTempPath();
std::string GetTempFileName( const char *szPrefix = 0 );

inline std::string GetCurrentDirectory()
{
    char szBuf[MAX_PATH];
    DWORD dwResult = ::GetCurrentDirectory( array_size(szBuf), szBuf );

    if ( dwResult < array_size(szBuf) )
        return szBuf;
    else if ( dwResult >= array_size(szBuf) )
        throw std::runtime_error( "::GetCurrentDirectory returns path that is too long" );
    else
        throw std::runtime_error( "::GetCurrentDirectory failed" );
}

inline std::string QuoteIfNecessary( std::string sFileName )
{
    if ( sFileName.find_first_of( ' ' ) != std::string::npos ) {
        sFileName.insert( 0, "\"" );
        sFileName += '\"';
    }

    return sFileName;
}

inline std::string sformat( const char *szFormat, ... )
{
    va_list args;
    char szBuf[4096];

    va_start( args, szFormat );
    size_t len = _vsnprintf( szBuf, array_size(szBuf)-1, szFormat, args );
    szBuf[len] = 0;

    return szBuf;
}

inline std::string GetTempPipeName()
{
    static int n;
    return sformat( "\\\\.\\pipe\\%ld_%d", GetCurrentProcessId(), ++n );
}

//==========================================================================>>
// RAII wrappers around Windows kernel objects
//==========================================================================>>

template <HANDLE INVALID_VALUE> class W32GenericHandle
{
public:
    W32GenericHandle( HANDLE h ) : h_(h) {}
    W32GenericHandle( W32GenericHandle& rhs ) : h_( rhs.Detach() ) {}
    W32GenericHandle& operator=( W32GenericHandle& rhs ) { W32GenericHandle tmp(rhs); swap(tmp); return *this; }
    ~W32GenericHandle() { Close(); } // Yes, not virtual
    
    void swap( W32GenericHandle& rhs ) { std::swap( h_, rhs.h_ ); }

    void Close() { if ( IsValid() ) ::CloseHandle( Detach() ); }
    bool operator!() const { return !IsValid(); } const
    bool IsValid() const { return h_ != INVALID_VALUE; }
    operator HANDLE() const { return h_; }

    HANDLE Detach() { HANDLE hToReturn = h_; h_ = INVALID_VALUE; return hToReturn; }

private:
    HANDLE h_;
};

typedef W32GenericHandle<INVALID_HANDLE_VALUE> W32Handle;

class W32Event
{
public:
    W32Event( bool bManualReset = true, bool bInitialState = false ) : HEvent_( ::CreateEvent( 0, bManualReset, bInitialState, 0 ) ) {}
    W32Event( W32Event& rhs ) : HEvent_( rhs.HEvent_ ) {}
    W32Event& operator=( W32Event& rhs ) { HEvent_ = rhs.HEvent_; return *this; }
    
    // Yes, implicit non-virtual desctructor

    void swap( W32Event& rhs ) { HEvent_.swap( rhs.HEvent_ ); }

    bool Reset() { return IsValid() && ::ResetEvent( HEvent_ ); }
    bool Set()   { return IsValid() && ::SetEvent( HEvent_ ); }
    void Close() { HEvent_.Close(); }

    bool operator!()  const { return !IsValid(); }
    bool IsValid()    const { return HEvent_.IsValid(); }
    operator HANDLE() const { return HEvent_; }

    HANDLE Detach() { return HEvent_.Detach(); }

private:
    W32GenericHandle<0> HEvent_;
};

//==========================================================================>>
// Scope guard for a temporary file
//==========================================================================>>

class TempFile
{
public:
    TempFile() : sFileName(GetTempFileName()) {}
    explicit TempFile( const char *szFileName ) : sFileName(szFileName) {}
    TempFile( TempFile& rhs ) : sFileName(rhs.Detach()) {}
    TempFile& operator=( TempFile& rhs ) { TempFile tmp(rhs); swap(tmp); return *this; }
    ~TempFile() { Delete(); } // Yes, not virtual

    bool IsValid()        const { return !sFileName.empty(); }
    bool operator!()      const { return !IsValid(); }
    std::string GetName() const { return sFileName; }

    void swap( TempFile& rhs ) { std::swap( sFileName, rhs.sFileName ); }

    void Delete() { if ( IsValid() ) ::DeleteFile( Detach().c_str() ); }
    std::string Detach() { std::string sToReturn(sFileName); sFileName.clear(); return sToReturn; }

private:
    std::string sFileName;
};

//==========================================================================>>
// Wrappers around FormatMessage
//==========================================================================>>

inline std::string FormatSystemMessage( DWORD dwMsgId )
{
    char buf[4096];
    FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, 0, dwMsgId, 0, buf, sizeof buf, 0 );

    int len = strlen(buf);

    if ( len > 1 && buf[len-2] == '\r' && buf[len-1] == '\n' )
        buf[len-2] = 0;

    return buf;
}

inline std::string LastErrorStr()
{
    return FormatSystemMessage( ::GetLastError() );
}

inline std::string GetLocalAppDataFolder()
{
    char szBuf[MAX_PATH];
    return SUCCEEDED( ::SHGetFolderPath( 0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, SHGFP_TYPE_CURRENT, szBuf ) ) ? szBuf : "";
}

#endif // __MISCUTIL_H
