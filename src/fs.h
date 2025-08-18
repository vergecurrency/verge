// Copyright (c) 2009-2017 The Bitcoin Core developers
// Copyright (c) 2018-2025 The Verge Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef VERGE_FS_H
#define VERGE_FS_H

#include <stdio.h>
#include <string>

// Modern C++ Migration: Use std::filesystem when C++17 is available
#if defined(ENABLE_CXX17) && __cplusplus >= 201703L && __has_include(<filesystem>)
    #include <filesystem>
    /** Filesystem operations and types */
    namespace fs = std::filesystem;
    
    // Define filesystem_error alias for compatibility
    using filesystem_error = std::filesystem::filesystem_error;
    
    #define VERGE_HAVE_STD_FILESYSTEM 1
#else
    // Fallback to boost::filesystem for C++14 compatibility
    #include <boost/filesystem.hpp>
    #include <boost/filesystem/fstream.hpp>
    #include <boost/filesystem/detail/utf8_codecvt_facet.hpp>
    
    /** Filesystem operations and types */
    namespace fs = boost::filesystem;
    
    // Define filesystem_error alias for compatibility
    using filesystem_error = boost::filesystem::filesystem_error;
    
    #define VERGE_HAVE_STD_FILESYSTEM 0
#endif

/** Bridge operations to C stdio */
namespace fsbridge {
    FILE *fopen(const fs::path& p, const char *mode);
    FILE *freopen(const fs::path& p, const char *mode, FILE *stream);
};

#endif // VERGE_FS_H
