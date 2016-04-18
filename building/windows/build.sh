#!/bin/bash

# build the windows exe

export CROSS=i686-w64-mingw32.static-
export CC=${CROSS}gcc
export CXX=${CROSS}g++
export LD=${CROSS}ld
export AR=${CROSS}ar
export PKG_CONFIG=${CROSS}pkg-config

which $CC
which $CXX
which $LD
which $AR
which $PKG

PATH=/usr/lib/mxe/usr/bin/:$PATH 

./autogen.sh --host=i686-w64-mingw32.static-

LDFLAGS='-lssl -lcrypto -static -L/usr/local/BerkeleyDB.4.8/lib' CFLAGS='-I/usr/local/BerkeleyDB.4.8/include/ -I/tmp/db-4.8.30.NC/build_windows' ./configure --host=i686-w64-mingw32.static --enable-static --disable-shared 

make

echo "done"
