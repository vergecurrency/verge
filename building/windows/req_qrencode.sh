#!/bin/bash

echo "=== Building QRENCODE now..."
cd /tmp/
wget https://fukuchi.org/works/qrencode/qrencode-3.4.4.tar.gz
tar xzf qrencode-3.4.4.tar.gz
cd /tmp/qrencode-3.4.4
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH ./configure --host=i686-w64-mingw32 --without-tools --enable-static --disable-shared
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH make
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH make install
echo "=== done building QRENCODE =="
