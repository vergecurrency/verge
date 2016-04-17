#!/bin/bash

echo "=== dpkg -l"
dpkg -l
# build Berkeley DB v4.8
echo "=== Building BDB now..."
cd /tmp
# Note: would be nice if 'apt-get source libdbd4.8' (or similar) would work
wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
tar xzf db-4.8.30.NC.tar.gz
cd /tmp/db-4.8.30.NC/build_unix
target=i686-w64-mingw32.static mxedir=/usr/lib/mxe/ PATH=/usr/lib/mxe/usr/bin/:$PATH CFLAGS="-m32 -mtune=generic" ../dist/configure --enable-cxx --enable-mingw --disable-replication --disable-shared --host=i686-w64-mingw32
target=i686-w64-mingw32.static mxedir=/usr/lib/mxe/ PATH=/usr/lib/mxe/usr/bin/:$PATH make library_build
target=i686-w64-mingw32.static mxedir=/usr/lib/mxe/ PATH=/usr/lib/mxe/usr/bin/:$PATH make install_lib install_include
echo "=== done building DB =="
