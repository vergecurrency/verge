#!/bin/bash

echo "=== Building QRENCODE now..."
cd /tmp/
wget https://fukuchi.org/works/qrencode/qrencode-3.4.4.tar.gz
tar xzf qrencode-3.4.4.tar.gz
cd /tmp/qrencode-3.4.4
./configure --host=${CROSS} --without-tools --enable-static --disable-shared
make
make install
echo "=== done building QRENCODE =="
