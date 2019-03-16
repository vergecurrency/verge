#!/bin/bash

set -e

echo "=== Building libevent now..."

cd /tmp/

wget https://github.com/libevent/libevent/releases/download/release-2.1.8-stable/libevent-2.1.8-stable.tar.gz

tar xf libevent-2.1.8-stable.tar.gz
cd libevent-2.1.8-stable
./configure --host=${CROSS} --enable-static --disable-shared --disable-openssl
make
make install
rm -rf /tmp/libevent-2.1.8-stable*

echo "=== Done building libevent ..."
