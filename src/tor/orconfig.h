#ifndef GENERIC_ORCONFIG_H_
#define GENERIC_ORCONFIG_H_

#if defined(_WIN32)
#  include "orconfig_win32.h"
#elif defined(__darwin__) || defined(__APPLE__)
#  include "orconfig_apple.h"
#else
#  include "orconfig_linux.h"
#endif

#endif  // GENERIC_ORCONFIG_H_
