#pragma once

#include <stdio.h>
#include <tchar.h>
#include <memory>
#include <functional>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/set.hpp>

#ifdef _UNICODE
typedef std::wstring tstring;
#else
typedef std::string tstring;
#endif

/// <summary>
/// A printf cousin returning a string.
/// </summary>
inline tstring sformat(const TCHAR *szFormat, ...)
{
    // Note the method is optimized for the optimistic case
    // when the result fits the static buffer.

    TCHAR staticBuf[2048];
    va_list args;

    va_start(args, szFormat);
    int len = _vsntprintf_s(staticBuf, _TRUNCATE, szFormat, args);
    va_end(args);

    if (len >= 0)
        return staticBuf;

    va_start(args, szFormat);
    len = _vsctprintf(szFormat, args) + 1; // +1 for trailing zero
    va_end(args);

    if (len <= 0)
        throw std::runtime_error("_vsctprintf() failed.");

    auto pDynamicBuf = std::unique_ptr<TCHAR[]>(new TCHAR[len]); // Should be changed to make_unique as soon as compiler provides it

    va_start(args, szFormat);
    _vsntprintf_s(pDynamicBuf.get(), len, _TRUNCATE, szFormat, args);
    va_end(args);

    return pDynamicBuf.get();
}

/// <summary>
/// Constructs new directory name from a base directory name and a
/// relative directory name.
/// </summary>
/// <remarks>
/// If the relative directory name is absolute, simply replaces
/// the base directory name.
/// If the relateive directory name is "..", the base directory name
/// is shortened to its parent directory name.
/// <p>
/// Defined in a header file because it is used in both projects
/// with static and dynamic runtimes.
/// </p>
/// </remarks>
inline tstring CatPath(const TCHAR *szDir, const TCHAR *szSubDir)
{
    if (szDir == 0 || *szDir == 0)
        return _T("");
    else if (szSubDir == 0 || *szSubDir == 0)
        return szDir;
    else if (szSubDir[1] == _T(':'))
        return szSubDir;
    else if (_tcschr(_T("\\/"), *szSubDir) != 0)
        return szDir[1] == _T(':') ? tstring(szDir, 2) + szSubDir : szSubDir;
    else if (_tcscmp(szSubDir, _T("..")) == 0)
    {
        const TCHAR *p = szDir + _tcslen(szDir) - 1;
        if (_tcschr(_T("\\/"), *p) != 0 && p > szDir && *(p - 1) != _T(':'))
            --p;
        while (p >= szDir && _tcschr(_T("\\/"), *p) == 0)
            --p;

        return p < szDir                         ? szDir :
               p == szDir || *(p - 1) == _T(':') ? tstring(szDir, p + 1 - szDir) :
                                                   tstring(szDir, p - szDir);
    }
    else
    {
        tstring sNewDir(szDir);
        if (!sNewDir.empty() && _tcschr(_T("\\/:"), *sNewDir.rbegin()) == 0)
            sNewDir += _T("\\");
        return sNewDir + szSubDir;
    }
}

/// <summary>
/// Extracts the filename from a full pathname, absolute or relative.
/// </summary>
/// <remarks>
/// Defined in a header file because it is used in both projects
/// with static and dynamic runtimes.
/// </remarks>
inline tstring ExtractFileName(const tstring& sPathName)
{
    size_t iLastSlash = sPathName.find_last_of(_T("\\/:"));
    return iLastSlash == tstring::npos ? sPathName : sPathName.substr(iLastSlash + 1);
}

/// <summary>
/// Extracts the path from a full pathname, absolute or relative.
/// </summary>
/// <remarks>
/// Defined in a header file because it is used in both projects
/// with static and dynamic runtimes.
/// </remarks>
inline tstring ExtractPath(const tstring& sPathName)
{
    size_t iLastSlash = sPathName.find_last_of(_T("\\/:"));
    return iLastSlash == tstring::npos ? _T("") : sPathName.substr(0, iLastSlash);
}

/// <summary>
/// Encloses pathname in double quotes, if necessary.
/// </summary>
/// <remarks>
/// Defined in a header file because it is used in both projects
/// with static and dynamic runtimes.
/// </remarks>
inline tstring QuoteIfNecessary(tstring sFileName)
{
    if (sFileName.find_first_of(_T(' ')) != tstring::npos)
    {
        sFileName.insert(0, _T("\""));
        sFileName += _T('\"');
    }

    return sFileName;
}

//==========================================================================>>
// String comparison functors
//==========================================================================>>

struct EqualNoCase : public std::binary_function<tstring, tstring, bool>
{
    result_type operator()(const first_argument_type& left, const second_argument_type& right) const
    {
        return _tcsicmp(left.c_str(), right.c_str()) == 0;
    }
};

struct LessNoCase : public std::binary_function<tstring, tstring, bool>
{
    result_type operator()(const first_argument_type& left, const second_argument_type& right) const
    {
        return _tcsicmp(left.c_str(), right.c_str()) < 0;
    }
};

struct StartsWithDir : public std::binary_function<tstring, tstring, bool>
{
    result_type operator()(const first_argument_type& str, const second_argument_type& substr) const
    {
        return _tcsnicmp(str.c_str(), substr.c_str(), substr.length()) == 0 &&
            (str.length() == substr.length() || _tcschr(_T("\\/:"), str[substr.length()]) != nullptr);
    }
};

struct IsFileFromDir : public std::binary_function<tstring, tstring, bool>
{
    result_type operator()(const first_argument_type& dirname, const second_argument_type& pathname) const
    {
        return _tcsnicmp(dirname.c_str(), pathname.c_str(), pathname.length()) == 0 &&
            pathname.find_last_of(_T("\\/:")) == dirname.length();
    }
};

/// <summary>
/// Returns a srting of exactly specified length by truncating the source string
/// with "..." or extending it with trailing spaces.
/// </summary>
inline tstring ProkrustString(const tstring& sVictim, tstring::size_type len)
{
    tstring s{ sVictim };

    if (s.length() > len)
    {
        static const TCHAR cszHellipsis[] = _T("...");
        s.erase(len - _countof(cszHellipsis) + 1) += cszHellipsis;
    }
    else if (s.length() < len)
        s += sformat(_T("%*c"), len - s.length(), _T(' '));

    return s;
}

/*
#include <string>
#include <vector>
#include <set>
#include <functional>
#include <algorithm>
#include <process.h>
#include <tchar.h>
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

template <typename C> struct s
{
};

template <> struct s<char>
{
    typedef std::string string;

    template <typename... Args> size_t strlen(Args... args) { return ::strlen(args...); }
    template <typename... Args> int vsnprintf(Args... args) { return ::vsnprintf(args...); }
};

template <> struct s<wchar_t>
{
    typedef std::wstring string;

    template <typename... Args> size_t strlen(Args... args) { return ::wcslen(args...); }
    template <typename... Args> int vsnprintf(Args... args) { return ::_vsnwprintf_s(args...); }
};

template <typename C> std::vector<typename s<C>::string> Split(const typename s<C>::string& s, C c);
void SafeSleep( DWORD dwMilliseconds );

template <typename C> inline typename s<C>::string GetModuleFileName(HMODULE hModule)
{
    C szFileName[MAX_PATH];
    int len = ::GetModuleFileName(hModule, szFileName, _countof(szFileName) - 1);
    szFileName[len] = 0;
    return szFileName;
}

template <typename C> inline typename s<C>::string u2s(unsigned int u)
{
    C buf[256];
    array_sprintf(buf, _T("%u"), u);
    return buf;
}

template <typename C> inline s<C>::string i2s(int i)
{
    C buf[256];
    array_sprintf(buf, _T("%d"), i);
    return buf;
}

template <typename C> inline s<C>::string l2s(long l)
{
    C buf[256];
    array_sprintf(buf, _T("%ld"), l);
    return buf;
}

template <typename C> inline s<C>::string ul2s(unsigned long ul)
{
    C buf[256];
    array_sprintf(buf, _T("%lu"), ul);
    return buf;
}

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
    explicit dir_iterator( const s<C>::string& sDir, bool bWithDots = false ) : paccessor_( new dir_accessor( sDir, bWithDots ) ) {}

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
        explicit dir_accessor( const s<C>::string& sDir, bool bWithDots ) : bWithDots_(bWithDots)
        {
            hFind_ = ::FindFirstFile( CatPath(sDir.c_str(), _T("*")).c_str(), &findData_ );
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

        bool is_dot_entry( const TCHAR *szName )
        {
            return _tcscmp( szName, _T(".") ) == 0 || _tcscmp( szName, _T("..") ) == 0;
        }
    };

    boost::intrusive_ptr<dir_accessor> paccessor_;
};

//==========================================================================>>
// GetTempPath/GetTempFileName wrappers
//==========================================================================>>

s<C>::string GetTempPath();
s<C>::string GetTempFileName( const TCHAR *szPrefix = 0 );

inline s<C>::string GetCurrentDirectory()
{
    TCHAR szBuf[MAX_PATH];
    DWORD dwResult = ::GetCurrentDirectory( _countof(szBuf), szBuf );

    if (dwResult < _countof(szBuf))
        return szBuf;
    else if (dwResult >= _countof(szBuf))
        throw std::runtime_error( "::GetCurrentDirectory returns path that is too long" );
    else
        throw std::runtime_error( "::GetCurrentDirectory failed" );
}

inline s<C>::string GetTempPipeName()
{
    static int n;
    return sformat( _T("\\\\.\\pipe\\%ld_%d"), GetCurrentProcessId(), ++n );
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
    explicit TempFile( const TCHAR *szFileName ) : sFileName(szFileName) {}

    TempFile( const TempFile& rhs ) : sFileName( const_cast<TempFile&>(rhs).Detach() ) {}      // Deliberate hack
    TempFile& operator=( const TempFile& rhs ) { TempFile tmp(rhs); swap(tmp); return *this; } // Here too

    ~TempFile() { Delete(); } // Yes, not virtual

    bool IsValid()    const { return !sFileName.empty(); }
    bool operator!()  const { return !IsValid(); }
    s<C>::string GetName() const { return sFileName; }

    void swap( TempFile& rhs ) { std::swap( sFileName, rhs.sFileName ); }

    void Delete() { if ( IsValid() ) ::DeleteFile( Detach().c_str() ); }
    s<C>::string Detach() { s<C>::string sToReturn(sFileName); sFileName.clear(); return sToReturn; }

private:
    s<C>::string sFileName;
};

//==========================================================================>>
// Wrappers around FormatMessage
//==========================================================================>>

inline s<C>::string FormatSystemMessage( DWORD dwMsgId )
{
    TCHAR buf[4096];
    FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, 0, dwMsgId, 0, buf, _countof(buf), 0 );

    size_t len = _tcslen(buf);

    if ( len > 1 && buf[len-2] == _T('\r') && buf[len-1] == _T('\n') )
        buf[len-2] = 0;

    return buf;
}

inline s<C>::string LastErrorStr()
{
    return FormatSystemMessage( ::GetLastError() );
}

inline s<C>::string GetLocalAppDataFolder()
{
    TCHAR szBuf[MAX_PATH];
    return SUCCEEDED( ::SHGetFolderPath( 0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, SHGFP_TYPE_CURRENT, szBuf ) ) ? szBuf : _T("");
}
*/