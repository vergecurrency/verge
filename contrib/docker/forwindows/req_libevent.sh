#!/bin/bash

set -e

echo "=== Building libevent now..."

cd /tmp/

LIBEVENT_VERSION=2.1.12-stable

wget https://github.com/libevent/libevent/releases/download/release-${LIBEVENT_VERSION}/libevent-${LIBEVENT_VERSION}.tar.gz

tar xf libevent-${LIBEVENT_VERSION}.tar.gz
cd libevent-${LIBEVENT_VERSION}
./configure --host=${CROSS} --enable-static --disable-shared --disable-openssl
make
make install
rm -rf /tmp/libevent-${LIBEVENT_VERSION}*

echo "=== Done building libevent ..."
