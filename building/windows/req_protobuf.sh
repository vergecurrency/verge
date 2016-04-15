#!/bin/bash

# build protobuf 
echo "=== Building PROTOBUF now..."
cd /tmp/
apt-get source libprotobuf-dev
cd protobuf-2.5.0
export PTW32_INCLUDE=/tmp/Pre-built.2/include
#export PTW32_LIB=/tmp/Pre-built.2/lib/x64
export PTW32_LIB=/tmp/Pre-built.2/lib/x86
./autogen.sh --host=i686-w64-mingw32
#./configure --host=i686-w64-mingw32 --prefix=/usr/i686-w64-mingw32 LDFLAGS='-static'
./configure --host=i686-w64-mingw32 --prefix=/usr/i686-w64-mingw32 --with-protoc=protoc LDFLAGS='-static'
sudo make CC=i686-w64-mingw32-gcc RANLIB=i686-w64-mingw32-ranlib AR=i686-w64-mingw32-ar
#PATH=$PATH:/usr/i686-w64-mingw32/bin sudo make install
echo "=== done building PROTOBUF =="
