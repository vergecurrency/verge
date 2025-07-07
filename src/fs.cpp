#include <fs.h>

namespace fsbridge {

FILE *fopen(const fs::path& p, const char *mode)
{
#if VERGE_HAVE_STD_FILESYSTEM
    // std::filesystem::path::string() returns std::string
    return ::fopen(p.string().c_str(), mode);
#else
    // boost::filesystem compatibility
    return ::fopen(p.string().c_str(), mode);
#endif
}

FILE *freopen(const fs::path& p, const char *mode, FILE *stream)
{
#if VERGE_HAVE_STD_FILESYSTEM
    // std::filesystem::path::string() returns std::string
    return ::freopen(p.string().c_str(), mode, stream);
#else
    // boost::filesystem compatibility
    return ::freopen(p.string().c_str(), mode, stream);
#endif
}

} // fsbridge
