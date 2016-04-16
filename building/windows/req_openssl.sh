#!/bin/bash

# build openssl
echo "=== Building OPENSSL now..."
cd /tmp/
sudo apt-get source openssl
# TODO: need to determine directory/version automatically
cd /tmp/openssl-1.0.1f
CROSS_COMPILE="x86_64-w64-mingw32-" sudo ./Configure mingw64 no-zlib no-asm no-shared no-dso no-asm --prefix=/usr/i686-w64-mingw32  
sudo make CC=i686-w64-mingw32-gcc 

echo "=== done building OPENSSL =="
