#include <networking/torencoding.h>

char* convertStringToCharArray(std::string &str){
    char *pc = new char[str.size()+1];
    std::strcpy(pc, str.c_str());
    return pc;
}