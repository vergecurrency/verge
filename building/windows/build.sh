#!/bin/bash

# build the windows exe

target=i686-w64-mingw32.static
mxedir=/usr/lib/mxe/

./autogen.sh --host=i686-w64-mingw32.static

PATH=/usr/lib/mxe/usr/bin/:$PATH env PTW32_INCLUDE=/tmp/pthreads.2 PTW32_LIB=/tmp/pthreads.2 SSL_CFLAGS='-L/tmp/openssl-1.0.1f/include' SSL_LIBS='-L/tmp/openssl-1.0.1f/lib' CFLAGS='-I/tmp/openssl-1.0.1f/include -I/tmp/db-4.8.30.NC/build_unix -I/tmp/boost_1_55_0 -I/tmp/miniupnpc-1.6 -I/tmp/pthreads.2' CRYPTO_CFLAGS='-L/tmp/openssl-1.0.1f/include' CRYPTO_LIBS='-L/tmp/openssl-1.0.1f/lib' CPPFLAGS='-I/tmp/openssl-1.0.1f/include -I/tmp/db-4.8.30.NC/build_unix -I/tmp/boost_1_55_0' LDFLAGS='-L/tmp/openssl-1.0.1f -L/tmp/db-4.8.30.NC/build_unix/.libs -L/tmp/boost_1_55_0/stage/lib -L/tmp/miniupnpc-1.6 -lssl -lcrypto -L/tmp/protobuf-2.5.0/src/.libs -L/tmp/pthreads.2' ./configure --with-boost-libdir=/tmp/boost_1_55_0/stage/lib --host=i686-w64-mingw32.static --enable-static --disable-shared
make

echo "done"
