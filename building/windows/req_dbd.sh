#!/bin/bash

# build Berkeley DB v4.8
echo "=== Building BDB now..."
cd /tmp
# Note: would be nice if 'apt-get source libdbd4.8' (or similar) would work
wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
tar xzf db-4.8.30.NC.tar.gz
cd /tmp/db-4.8.30.NC/build_unix
PATH=$PATH:/usr/i686-w64-mingw32/bin sh ../dist/configure --enable-cxx --enable-mingw --disable-replication
PATH=$PATH:/usr/i686-w64-mingw32/bin make
echo "=== done bilding DB =="
