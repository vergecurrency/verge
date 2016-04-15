#!/bin/bash

# build the windows exe

./autogen.sh --host=i686-w64-mingw32 
PATH=$PATH:/usr/i686-w64-mingw32/bin env SSL_CFLAGS='-L/tmp/openssl-1.0.1f/include' SSL_LIBS='-L/tmp/openssl-1.0.1f/lib' CFLAGS='-I/tmp/openssl-1.0.1f/include -I/tmp/db-4.8.30/build_unix -I/tmp/boost_1_55_0 -I/tmp/miniupnpc-1.6' LDFLAGS='-L/tmp/openssl-1.0.1f -L/tmp/db-4.8.30/build_unix/.libs -L/tmp/boost_1_55_0/stage/lib' CRYPTO_CFLAGS='-L/tmp/openssl-1.0.1f/include' CRYPTO_LIBS='-L/tmp/openssl-1.0.1f/lib' CPPFLAGS='-I/tmp/openssl-1.0.1f/include -I/tmp/db-4.8.30/build_unix -I/tmp/boost_1_55_0' LDFLAGS='-L/tmp/openssl-1.0.1f -L/tmp/db-4.8.30/build_unix/.libs -L/tmp/boost_1_55_0/stage/lib -L/tmp/miniupnpc-1.6 -lssl -lcrypto' ./configure --host=i686-w64-mingw32 --with-boost-libdir=/tmp/boost_1_55_0/stage/lib 

PATH=$PATH:/usr/i686-w64-mingw32/bin AR=i686-w64-mingw32-ar make
