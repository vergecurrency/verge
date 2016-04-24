#!/bin/bash

# build the windows exe

./autogen.sh --host=i686-w64-mingw32.static-

LDFLAGS='-lssl -lcrypto -static -L/usr/local/BerkeleyDB.4.8/lib' CFLAGS='-I/usr/local/BerkeleyDB.4.8/include/ -I/tmp/db-4.8.30.NC/build_windows' ./configure --host=i686-w64-mingw32.static --enable-static --disable-shared 

make

echo "done"
