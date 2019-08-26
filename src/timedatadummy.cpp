#include <stdint.h>
#include <ctime>

// This is for the consensus lib to also build while
// it has no access to the real timedata when compiling. 
// Thus we need this dummy time method to prevent any 
// further compiling issues.
int64_t GetAdjustedTime()
{
    return std::time(nullptr);
}
