#!/bin/bash

echo "=== Building ZLIB now..."
cd /tmp/
wget http://zlib.net/zlib-1.2.8.tar.gz
tar xzf zlib-1.2.8.tar.gz
cd zlib-1.2.8
CC=x86_64-w64-mingw32-gcc AR=x86_64-w64-mingw32-ar RANLIB=x86_64-w64-mingw32-ranlib ./configure --prefix=/usr/x86_64-w64-mingw32 --static
make
echo "=== done building ZLIB =="
