#pragma once

#include <boost/iterator/iterator_facade.hpp>
#include <boost/intrusive_ptr.hpp>
#include <shlobj.h>
#include "miscutil.h"

//==========================================================================>>
// Critical section wrapper and guard
//==========================================================================>>

class CriticalSection final
{
public:
    CriticalSection() { ::InitializeCriticalSection(&cs); }
    ~CriticalSection() { ::DeleteCriticalSection(&cs); }

    CriticalSection(const CriticalSection&) = delete;
    CriticalSection& operator=(const CriticalSection&) = delete;

    void Enter() { ::EnterCriticalSection(&cs); }
    void Leave() { ::LeaveCriticalSection(&cs); }

private:
    CRITICAL_SECTION cs;
};

class CSGuard final
{
public:
    CSGuard(CriticalSection& _cs) : cs(_cs) { cs.Enter(); }
    ~CSGuard() { cs.Leave(); }

    CSGuard(const CSGuard&) = delete;
    CSGuard& operator=(const CSGuard&) = delete;

private:
    CriticalSection& cs;
};

//==========================================================================>>
// Thread class. Encapsulates thread starting and graceful stopping
//==========================================================================>>

class Thread
{
public:
    typedef unsigned int ThreadRoutine(void *p, HANDLE hTerminateEvent);

    Thread(ThreadRoutine *pThreadRoutine) : m_pThreadRoutine(pThreadRoutine), m_hTerminateEvent(0), m_hThread(0) {}
    virtual ~Thread() { Stop(); }
    HRESULT Start(void *Param);
    void Stop(unsigned long dwMilliseconds = 1000);

    Thread(const Thread&) = delete;
    Thread& operator=(const Thread&) = delete;

private:
    HANDLE m_hThread;
    HANDLE m_hTerminateEvent;
    ThreadRoutine *m_pThreadRoutine;
    void *m_Param;

    static unsigned int __stdcall ThreadWrapper(void *pThread)
    {
        Thread& thread = *static_cast<Thread*>(pThread);
        return thread.m_pThreadRoutine(thread.m_Param, thread.m_hTerminateEvent);
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

class dir_iterator : public boost::iterator_facade<dir_iterator, WIN32_FIND_DATA, boost::single_pass_traversal_tag>
{
public:
    dir_iterator() {}
    explicit dir_iterator(const tstring& sDir, bool bWithDots = false) : paccessor_(new dir_accessor(sDir, bWithDots)) {}

private:
    friend class boost::iterator_core_access;

    void increment()
    {
        paccessor_->advance();
    }

    bool equal(const dir_iterator& rhs) const
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
        explicit dir_accessor(const tstring& sDir, bool bWithDots) : bWithDots_(bWithDots)
        {
            hFind_ = ::FindFirstFile(CatPath(sDir.c_str(), _T("*")).c_str(), &findData_);
            if (hFind_ == INVALID_HANDLE_VALUE && ::GetLastError() != ERROR_NO_MORE_FILES )
                throw std::runtime_error("::FindFirstFile failed");
            if (!bWithDots_ && is_dot_entry(findData_.cFileName))
                advance();
        }

        ~dir_accessor()
        {
            release_handle();
        }

        void advance()
        {
            if (hFind_ == INVALID_HANDLE_VALUE)
                throw std::runtime_error("Invalid attempt to advance to the next item in dir_accessor");

            if (!::FindNextFile(hFind_, &findData_))
                if (::GetLastError() == ERROR_NO_MORE_FILES)
                    release_handle();
                else
                    throw std::runtime_error("::FindNextFile failed");

            if (!bWithDots_ && is_dot_entry(findData_.cFileName))
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
            if (hFind_ != INVALID_HANDLE_VALUE && !::FindClose(hFind_))
                throw std::runtime_error("::FindClose failed");

            hFind_ = INVALID_HANDLE_VALUE;
        }

        bool is_dot_entry(const TCHAR *szName)
        {
            return _tcscmp(szName, _T(".")) == 0 || _tcscmp(szName, _T("..")) == 0;
        }
    };

    boost::intrusive_ptr<dir_accessor> paccessor_;
};

//==========================================================================>>
// GetTempPath/GetTempFileName wrappers
//==========================================================================>>

tstring GetTempPath();
tstring GetTempFileName(const TCHAR *szPrefix = 0);

inline tstring GetCurrentDirectory()
{
    TCHAR szBuf[MAX_PATH];
    DWORD dwResult = ::GetCurrentDirectory(_countof(szBuf), szBuf);

    if (dwResult < _countof(szBuf))
        return szBuf;
    else if (dwResult >= _countof(szBuf))
        throw std::runtime_error("::GetCurrentDirectory returns a path longer than MAX_PATH");
    else
        throw std::runtime_error("::GetCurrentDirectory failed");
}

inline tstring GetTempPipeName()
{
    static int n;
    return sformat(_T("\\\\.\\pipe\\%ld_%d"), GetCurrentProcessId(), ++n);
}

//==========================================================================>>
// RAII wrappers around Windows kernel objects
//==========================================================================>>

template <HANDLE INVALID_VALUE> class W32GenericHandle final
{
public:
    W32GenericHandle(HANDLE h) : h_(h) {}
    W32GenericHandle(W32GenericHandle&& rhs) : h_(rhs.Detach()) {}
    W32GenericHandle& operator=(W32GenericHandle&& rhs) { swap(*this, W32GenericHandle(rhs)); return *this; }
    ~W32GenericHandle() { Close(); } // Yes, not virtual: the class is final.
    
    friend void swap(W32GenericHandle& left, W32GenericHandle& right)
    {
        using std::swap;
        swap(left.h_, right.h_ );
    }

    void Close() { if (IsValid()) ::CloseHandle(Detach()); }
    bool operator!() const { return !IsValid(); }
    bool IsValid() const { return h_ != INVALID_VALUE; }
    operator HANDLE() const { return h_; }

    HANDLE Detach() { HANDLE hToReturn = h_; h_ = INVALID_VALUE; return hToReturn; }

private:
    HANDLE h_;
};

typedef W32GenericHandle<INVALID_HANDLE_VALUE> W32Handle;

class W32Event final
{
public:
    W32Event(bool bManualReset = true, bool bInitialState = false) : HEvent_(::CreateEvent(0, bManualReset, bInitialState, 0)) {}
    W32Event(W32Event&& rhs) : HEvent_(std::move(rhs.HEvent_)) {}
    W32Event& operator=(W32Event&& rhs) { swap(*this, rhs); return *this; }
    
    // Yes, implicit non-virtual desctructor

    friend void swap(W32Event& left, W32Event& right)
    {
        using std::swap;
        swap(left.HEvent_, right.HEvent_);
    }

    bool Reset() { return IsValid() && ::ResetEvent(HEvent_); }
    bool Set()   { return IsValid() && ::SetEvent(HEvent_); }
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

class TempFile final
{
public:
    TempFile() : sFileName(GetTempFileName()) {}
    explicit TempFile(const TCHAR *szFileName) : sFileName(szFileName) {}

    TempFile(TempFile&& rhs) : sFileName(std::move(rhs).Detach()) {}
    TempFile& operator=(TempFile&& rhs) { swap(TempFile(rhs), *this); return *this; }

    ~TempFile() { Delete(); } // Yes, not virtual: the class is final

    bool IsValid() const { return !sFileName.empty(); }
    bool operator!() const { return !IsValid(); }
    const TCHAR *GetName() const { return sFileName.c_str(); }

    friend void swap(TempFile& left, TempFile& right)
    {
        using std::swap;
        swap(left.sFileName, right.sFileName);
    }

    void Delete() { if (IsValid()) ::DeleteFile(Detach().c_str()); }
    tstring Detach() { tstring sToReturn(sFileName); sFileName.clear(); return sToReturn; }

private:
    tstring sFileName;
};

//==========================================================================>>
// Wrappers around FormatMessage
//==========================================================================>>

inline tstring FormatSystemMessage(DWORD dwMsgId)
{
    TCHAR buf[2048];
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, dwMsgId, 0, buf, _countof(buf), 0);

    size_t len = _tcslen(buf);

    if (len > 1 && buf[len-2] == _T('\r') && buf[len-1] == _T('\n'))
        buf[len-2] = 0;

    return buf;
}

inline tstring LastErrorStr()
{
    return FormatSystemMessage(::GetLastError());
}

inline tstring GetLocalAppDataFolder()
{
    TCHAR szBuf[MAX_PATH];
    return SUCCEEDED(::SHGetFolderPath(0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, SHGFP_TYPE_CURRENT, szBuf)) ? szBuf : _T("");
}

/// <summary>
/// printf-like wrapper around ::OutputDebugString.
/// </summary>
inline void Log(const TCHAR *szFormat, ...)
{
    va_list args;
    TCHAR szBuf[2048];

    va_start(args, szFormat);
    long len = _vsntprintf_s(szBuf, _TRUNCATE, szFormat, args);
    va_end(args);

    ::OutputDebugString(szBuf);
}
