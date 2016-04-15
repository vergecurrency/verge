#!/bin/bash

# build the windows exe

./autogen.sh --host=i686-w64-mingw32
env CPPFLAGS='-I/tmp/openssl-1.0.1f/include -I/tmp/db-4.8.30/build_unix -I/tmp/boost_1_55_0' LDFLAGS='-L/tmp/openssl-1.0.1f -L/tmp/db-4.8.30/build_unix/.libs -L/tmp/boost_1_55_0/stage/lib' ./configure --host=i686-w64-mingw32 --with-boost-libdir=/tmp/boost_1_55_0/stage/lib

make
