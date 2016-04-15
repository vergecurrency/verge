#!/bin/bash

# build libevent
echo "=== Building libevent now..."
cd /tmp/
apt-get source libevent-dev
cd /tmp/libevent-2.0.21-stable
./autogen.sh --host=i686-w64-mingw32
./configure --host=i686-w64-mingw32 --prefix=/usr/i686-w64-mingw32 LDFLAGS='-static'
PATH=$PATH:/usr/i686-w64-mingw32/bin make 
PATH=$PATH:/usr/i686-w64-mingw32/bin make install
