#!/bin/bash

# build Berkeley DB v4.8
echo "=== Building BDB now..."
cd /tmp
# Note: would be nice if 'apt-get source libdbd4.8' (or similar) would work
wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
tar xzf db-4.8.30.NC.tar.gz
cd /tmp/db-4.8.30.NC/build_unix
CC=/usr/bin/i686-w64-mingw32-gcc LD=/usr/bin/i686-w64-mingw32-ld CFLAGS="-mtune=i386 -m32 -Wl,--oformat,pei-i386 " ../dist/configure --enable-cxx --enable-mingw --disable-replication --disable-shared --host=i686-w64-mingw32
make library_build
make install_lib install_include
echo "=== done building DB =="
