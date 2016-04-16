#!/bin/bash

# build openssl
echo "=== Building OPENSSL now..."
cd /tmp/
sudo apt-get source openssl
# TODO: need to determine directory/version automatically
cd /tmp/openssl-1.0.1f
#CROSS_COMPILE="x86_64-w64-mingw32-" sudo ./Configure mingw64 no-asm no-shared --prefix=/usr/i686-w64-mingw32  
CROSS_COMPILE="x86_64-w64-mingw32-" ./Configure mingw64 no-asm shared --prefix=/opt/mingw64
PATH=$PATH:/usr/i686-w64-mingw32/bin make depend
PATH=$PATH:/usr/i686-w64-mingw32/bin make 

echo "=== done building OPENSSL =="
