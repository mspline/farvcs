#pragma once

#include <functional>
#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/set.hpp>
#include "miscutil.h"

/// <summary>
/// Thread-safe set of full file names.
/// </summary>
class TSFileSet
{
public:
    typedef std::set<tstring, LessNoCase> UnderlyingSetType;
    typedef UnderlyingSetType::iterator iterator;
    typedef UnderlyingSetType::const_iterator const_iterator;

    iterator begin() { return cont.begin(); }
    iterator end()   { return cont.end(); }

    const_iterator begin() const { return cont.begin(); }
    const_iterator end()   const { return cont.end(); }

public:
    void Add(const tstring& sFile)               { CSGuard _(cs); cont.insert(sFile); }
    void Remove(const tstring& sFile)            { CSGuard _(cs); cont.erase(sFile); }
    bool Contains(const tstring& sFile) const    { CSGuard _(cs); return cont.find(sFile) != cont.end(); }
    bool ContainsDown(const tstring& sDir) const { CSGuard _(cs); return std::find_if(cont.begin(), cont.end(), std::bind2nd(StartsWithDir(), sDir)) != cont.end(); }
    void Merge(const TSFileSet& rhs)             { CSGuard _(cs); cont.insert(rhs.cont.begin(), rhs.cont.end()); }
    void Clear()                                 { CSGuard _(cs); cont.clear(); }

    void RemoveFilesOfDir(const tstring& sDir, bool bRecursive)
    {
        CSGuard _(cs);

        for (UnderlyingSetType::iterator p = cont.begin(); p != cont.end(); )
        {
            if (bRecursive ? std::bind2nd(StartsWithDir(), sDir)(*p) : std::bind2nd(IsFileFromDir(), sDir)(*p))
                cont.erase(p++);
            else
                ++p;
        }
    }

private:
    UnderlyingSetType cont;
    mutable CriticalSection cs;

private:
    friend class boost::serialization::access;
    template <class Archive> void serialize(Archive& ar, const unsigned int) { CSGuard _(cs); ar & cont; }
};
