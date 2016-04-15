#!/bin/bash

# build openssl
echo "=== Building openssl now..."
cd /tmp/
sudo apt-get source openssl
# TODO: need to determine directory/version automatically
cd /tmp/openssl-1.0.1f
CROSS_COMPILE="x86_64-w64-mingw32-" ./Configure mingw64 no-zlib no-asm no-shared no-dso no-asm --prefix=/usr/i686-w64-mingw32  
make CC=i686-w64-mingw32-gcc RANLIB=i686-w64-mingw32-ranlib
#PATH=$PATH:/usr/i686-w64-mingw32/bin make depend
#PATH=$PATH:/usr/i686-w64-mingw32/bin make
#PATH=$PATH:/usr/i686-w64-mingw32/bin sudo make install

