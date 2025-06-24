// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_WARNINGS_H
#define VERGE_WARNINGS_H

#include <stdlib.h>
#include <string>

// Modern C++ Migration: Performance optimization with string_view
#if defined(ENABLE_CXX17) && __cplusplus >= 201703L
    #include <string_view>
    void SetMiscWarning(std::string_view strWarning);
#else
    void SetMiscWarning(const std::string& strWarning);
#endif
void SetfLargeWorkForkFound(bool flag);
bool GetfLargeWorkForkFound();
void SetfLargeWorkInvalidChainFound(bool flag);
/** Format a string that describes several potential problems detected by the core.
 * strFor can have three values:
 * - "statusbar": get all warnings
 * - "gui": get all warnings, translated (where possible) for GUI
 * This function only returns the highest priority warning of the set selected by strFor.
 */
#if defined(ENABLE_CXX17) && __cplusplus >= 201703L
    std::string GetWarnings(std::string_view strFor);
#else
    std::string GetWarnings(const std::string& strFor);
#endif

#endif //  VERGE_WARNINGS_H
