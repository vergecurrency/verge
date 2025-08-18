
// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sync.h>
#include <clientversion.h>
#include <util/system.h>
#include <warnings.h>

// Modern C++ Migration: Performance optimization with string_view
#if defined(ENABLE_CXX17) && __cplusplus >= 201703L
    #include <string_view>
#endif

// Modern C++ Migration: Use non-recursive mutex for better performance  
VergeStdMutex cs_warnings;
std::string strMiscWarning GUARDED_BY(cs_warnings);
bool fLargeWorkForkFound GUARDED_BY(cs_warnings) = false;
bool fLargeWorkInvalidChainFound GUARDED_BY(cs_warnings) = false;

#if defined(ENABLE_CXX17) && __cplusplus >= 201703L
void SetMiscWarning(std::string_view strWarning)
{
    // Modern C++ Migration: Use LOCK_GUARD and string_view for performance
    LOCK_GUARD(cs_warnings);
    strMiscWarning = strWarning; // string_view converts implicitly to string
}
#else
void SetMiscWarning(const std::string& strWarning)
{
    // Modern C++ Migration: Use LOCK_GUARD for simple cases
    LOCK_GUARD(cs_warnings);
    strMiscWarning = strWarning;
}
#endif

void SetfLargeWorkForkFound(bool flag)
{
    LOCK_GUARD(cs_warnings);
    fLargeWorkForkFound = flag;
}

bool GetfLargeWorkForkFound()
{
    LOCK(cs_warnings);
    return fLargeWorkForkFound;
}

void SetfLargeWorkInvalidChainFound(bool flag)
{
    LOCK(cs_warnings);
    fLargeWorkInvalidChainFound = flag;
}

#if defined(ENABLE_CXX17) && __cplusplus >= 201703L
std::string GetWarnings(std::string_view strFor)
{
    std::string strStatusBar;
    std::string strGUI;
    const std::string uiAlertSeperator = "<hr />";

    LOCK(cs_warnings);

    if (!CLIENT_VERSION_IS_RELEASE) {
        strStatusBar = "This is a pre-release test build - use at your own risk - do not use for mining or merchant applications";
        strGUI = _("This is a pre-release test build - use at your own risk - do not use for mining or merchant applications");
    }

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        strStatusBar = strMiscWarning;
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + strMiscWarning;
    }

    if (fLargeWorkForkFound)
    {
        strStatusBar = "Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.";
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + _("Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.");
    }
    else if (fLargeWorkInvalidChainFound)
    {
        strStatusBar = "Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.";
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + _("Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.");
    }

    // Modern C++ Migration: string_view supports efficient string comparison
    if (strFor == "gui")
        return strGUI;
    else if (strFor == "statusbar")
        return strStatusBar;
    assert(!"GetWarnings(): invalid parameter");
    return "error";
}
#else
std::string GetWarnings(const std::string& strFor)
{
    std::string strStatusBar;
    std::string strGUI;
    const std::string uiAlertSeperator = "<hr />";

    LOCK(cs_warnings);

    if (!CLIENT_VERSION_IS_RELEASE) {
        strStatusBar = "This is a pre-release test build - use at your own risk - do not use for mining or merchant applications";
        strGUI = _("This is a pre-release test build - use at your own risk - do not use for mining or merchant applications");
    }

    // Misc warnings like out of disk space and clock is wrong
    if (strMiscWarning != "")
    {
        strStatusBar = strMiscWarning;
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + strMiscWarning;
    }

    if (fLargeWorkForkFound)
    {
        strStatusBar = "Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.";
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + _("Warning: The network does not appear to fully agree! Some miners appear to be experiencing issues.");
    }
    else if (fLargeWorkInvalidChainFound)
    {
        strStatusBar = "Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.";
        strGUI += (strGUI.empty() ? "" : uiAlertSeperator) + _("Warning: We do not appear to fully agree with our peers! You may need to upgrade, or other nodes may need to upgrade.");
    }

    if (strFor == "gui")
        return strGUI;
    else if (strFor == "statusbar")
        return strStatusBar;
    assert(!"GetWarnings(): invalid parameter");
    return "error";
}
#endif
