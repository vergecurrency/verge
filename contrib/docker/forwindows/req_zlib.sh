#!/bin/bash

set -e

echo "=== Building ZLIB now..."

cd /tmp
wget http://zlib.net/fossils/zlib-1.2.11.tar.gz
tar xvfz zlib-1.2.11.tar.gz
cd zlib-1.2.11

# sed -e s/"PREFIX ="/"PREFIX = i686-w64-mingw32.static-"/ -i win32/Makefile.gcc

if [ "$TARGET_PLATFORM" == "win64" ]
then
  echo "The target platform is win64"
  sed -e s/"PREFIX ="/"PREFIX = x86_64-w64-mingw32-"/ -i win32/Makefile.gcc
elif [ "$TARGET_PLATFORM" == "win32" ]
then
  echo "The target platform is win32"
  sed -e s/"PREFIX ="/"PREFIX = i686-w64-mingw32-"/ -i win32/Makefile.gcc
else
  echo "You should setup TARGET_PLATFORM environment variable. It can be win64 or win32"
  exit 1
fi

make -f win32/Makefile.gcc 

echo "=== Done building ZLIB ..."
