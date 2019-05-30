#!/bin/bash

set -e

echo "=== Building OPENSSL now..."

cd /tmp/
add-apt-repository -s "deb http://archive.ubuntu.com/ubuntu/ bionic main restricted"
apt-get update
apt-get source openssl
openssl_version=`dpkg -s openssl | grep Version | sed 's/Version: \(.*\)-.*/\1/'`
cd /tmp/openssl-${openssl_version}

#./Configure mingw64 -m32 --cross-compile-prefix=i686-w64-mingw32.static- no-zlib no-asm no-shared no-dso no-asm 

if [ "$TARGET_PLATFORM" == "win64" ]
then
  echo "The target platform is win64"
  ./Configure mingw64 no-zlib no-asm no-shared no-dso no-asm
elif [ "$TARGET_PLATFORM" == "win32" ]
then
  echo "The target platform is win32"
  ./Configure mingw64 -m32 no-zlib no-asm no-shared no-dso no-asm
else
  echo "You should setup TARGET_PLATFORM environment variable. It can be win64 or win32"
  exit 1
fi

# skip the tests as they build exe files that will not run on linux
sed -i 's/TESTS = alltests/TESTS = /' Makefile
rm -rf certs/demo
make
make install_sw
rm -rf /tmp/openssl*

echo "=== Done building OPENSSL =="
