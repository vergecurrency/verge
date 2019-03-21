#!/bin/bash

set -e

echo "=== Building QRENCODE now..."

cd /tmp/
wget https://fukuchi.org/works/qrencode/qrencode-3.4.4.tar.gz
tar xzf qrencode-3.4.4.tar.gz
cd /tmp/qrencode-3.4.4
./configure --host=${CROSS} --without-tools --enable-static --disable-shared
make
make install
rm -rf qrencode-3.4.4*

echo "=== Done building QRENCODE =="
