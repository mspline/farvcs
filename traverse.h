#pragma once

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
    Traversal(const TCHAR *szPluginName, const TCHAR *szDir) : SimpleLongOperation(szPluginName),
        m_szDir(szDir)
    {}

protected:
    virtual intptr_t DoGetPrompt() { return M_Traversing; }

    virtual const TCHAR *DoGetInitialText() override
    {
        _tcsncpy_s(m_szTruncatedDir, m_szDir, _TRUNCATE);
        FSF.TruncPathStr(m_szTruncatedDir, W());
        return m_szTruncatedDir;
    }

    virtual bool DoExecute()
    {
        static int cnPreCountedDepth = 2;
        int nPreCountedDirs = CountVcsDirs(m_szDir, cnPreCountedDepth);

        unsigned long dirCount = 0; // Accumulates the number of the visited directories

        return Traverse(m_szDir, nPreCountedDirs, cnPreCountedDepth, 0, dirCount);
    }

private:
    bool Traverse(const tstring& sDir, int nPreCountedDirs, int nPreCountedDepth, int nLevel, unsigned long& dirCount)
    {
        // Read the VCS data (does nothing if not in a VCS-controlled directory)

        if (!IsVcsDir(sDir))
            return true;

        boost::intrusive_ptr<IVcsData> pVcsData = GetVcsData(sDir);

        // Enumerate all the entries in the current directory

        bool bDirtyFilesExist = false;
        bool bRetValue = true;

        for (const auto& entry : pVcsData->entries())
        {
            if (entry.first == _T(".."))
                continue;

            bDirtyFilesExist |= IsFileDirty(entry.second.status);

            tstring sPathName = CatPath(sDir.c_str(), entry.first.c_str());

            if ((entry.second.fileFindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IsVcsDir(sPathName))
            {
                if (nLevel < nPreCountedDepth)
                    ++dirCount;

                if (UserInteraction(sPathName.c_str(), static_cast<unsigned short>(dirCount * 100 / nPreCountedDirs)) && (bRetValue = false, true))
                    break;

                bRetValue = Traverse(sPathName.c_str(), nPreCountedDirs, nPreCountedDepth, nLevel + 1, dirCount);

                if (!bRetValue)
                    break;
            }
        }

        if (bDirtyFilesExist)
            DirtyDirs.Add(sDir);
        else
            DirtyDirs.Remove(sDir);

        return bRetValue;
    }

    const TCHAR *m_szDir;
    TCHAR m_szTruncatedDir[MAX_PATH]; // Used in DoGetInitialInfo method only, but stored here because of lifetime
};
