#!/bin/bash

# build libevent
echo "=== Building libevent now..."
cd /tmp/
apt-get source libevent-dev > /dev/null
cd /tmp/libevent-2.0.21-stable
./autogen.sh --host=i686-w64-mingw32
./configure --host=i686-w64-mingw32 --prefix=/usr/i686-w64-mingw32 LDFLAGS='-static'
sudo make CC=i686-w64-mingw32-gcc RANLIB=i686-w64-mingw32-ranlib AR=i686-w64-mingw32-ar
#PATH=$PATH:/usr/i686-w64-mingw32/bin sudo make install
echo "=== done building libevent =="
