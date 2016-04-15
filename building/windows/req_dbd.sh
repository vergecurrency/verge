#!/bin/bash

# build Berkeley DB v4.8
echo "=== Building BDB now..."
cd /tmp
# Note: would be nice if 'apt-get source libdbd4.8' (or similar) would work
wget 'https://launchpad.net/~bitcoin/+archive/ubuntu/bitcoin/+files/db4.8_4.8.30.orig.tar.gz' -O db4.8_4.8.30.orig.tar.gz
# Note: Could pull it from Oracle like this: wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
tar xzf db4.8_4.8.30.orig.tar.gz
cd /tmp/db-4.8.30/build_unix
PATH=$PATH:/usr/i686-w64-mingw32/bin sh ../dist/configure --host=i686-w64-mingw32 --enable-cxx --enable-mingw --disable-replication --prefix=/usr/i686-w64-mingw32
sudo make CC=i686-w64-mingw32-gcc RANLIB=i686-w64-mingw32-ranlib AR=i686-w64-mingw32-ar
#PATH=$PATH:/usr/i686-w64-mingw32/bin sudo make install

echo "=== done bilding BDB =="
