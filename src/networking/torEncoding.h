#ifndef VERGE_NETWORKING_TORENCODING_H
#define VERGE_NETWORKING_TORENCODING_H

#include <string>

static char* convertStringToCharArray(std::string &str){
    char *pc = new char[str.size()+1];
    std::strcpy(pc, str.c_str());
    return pc;
}

#endif // VERGE_NETWORKING_TORENCODING_H