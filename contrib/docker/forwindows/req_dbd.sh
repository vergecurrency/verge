#!/bin/bash

set -e

echo "=== Building BDB now..."

cd /tmp
# Note: would be nice if 'apt-get source libdbd4.8' (or similar) would work
wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
tar xzf db-4.8.30.NC.tar.gz
cd /tmp/db-4.8.30.NC/build_windows

# CFLAGS="-m32 -mtune=generic" ../dist/configure --enable-cxx --enable-mingw --host=i686-w64-mingw32.static  --disable-replication

if [ "$TARGET_PLATFORM" == "win64" ]
then
  echo "The target platform is win64"
  CFLAGS="-mtune=generic" ../dist/configure --enable-cxx --enable-mingw --host=${CROSS}  --disable-replication
elif [ "$TARGET_PLATFORM" == "win32" ]
then
  echo "The target platform is win32"
  CFLAGS="-m32 -mtune=generic" ../dist/configure --enable-cxx --enable-mingw --host=${CROSS}  --disable-replication
else
  echo "You should setup TARGET_PLATFORM environment variable. It can be win64 or win32"
  exit 1
fi

/bin/sed -i 's/#include <direct.h>//' ../dist/../dbinc/win_db.h
make
make library_build
make install_lib install_include
rm -rf /tmp/db-4.8.30.NC*

echo "=== Done building DB =="
