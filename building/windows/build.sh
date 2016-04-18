#!/bin/bash

# build the windows exe

./autogen.sh --host=i686-w64-mingw32.static

target=i686-w64-mingw32.static mxedir=/usr/lib/mxe/ PATH=/usr/lib/mxe/usr/bin/:$PATH 
LDFLAGS='-lssl -lcrypto' ./configure --with-boost-libdir=/tmp/boost_1_55_0/stage/lib --host=i686-w64-mingw32 --enable-static --disable-shared
PATH=/usr/lib/mxe/usr/bin/:$PATH make

echo "done"
