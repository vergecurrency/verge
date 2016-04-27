#!/bin/bash

# build the windows exe

export LDFLAGS='-lssl -lcrypto -static -L/tmp/db-4.8.30.NC/build_windows -L/tmp/boost_1_55_0/stage/lib -L/tmp/miniupnpc-1.9.20160209 -L/tmp/protobuf-2.5.0/src/.libs/' 
export CFLAGS='-I/tmp/db-4.8.30.NC/build_windows -I/tmp/boost_1_55_0/boost -I/tmp -I/tmp/protobuf-2.5.0'
export CXXFLAGS=$CFLAGS
export CPPFLAGS=$CFLAGS
export BOOST_ROOT=/tmp/boost_1_55_0

./autogen.sh --host=i686-w64-mingw32.static-

./configure --host=i686-w64-mingw32.static 
#./configure --host=i686-w64-mingw32.static --with-gui=qt5

make

echo "done"
