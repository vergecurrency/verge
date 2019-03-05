#!/bin/bash

# build openssl
echo "=== Building OPENSSL now..."
cd /tmp/
apt-get source openssl
openssl_version=`dpkg -s openssl | grep Version | sed 's/Version: \(.*\)-.*/\1/'`
cd /tmp/openssl-${openssl_version}
#./Configure mingw64 -m32 --cross-compile-prefix=i686-w64-mingw32.static- no-zlib no-asm no-shared no-dso no-asm 
./Configure mingw64 -m32 no-zlib no-asm no-shared no-dso no-asm 
#sudo ./Configure mingw64 -m32 no-zlib no-asm no-shared no-dso no-asm 
# skip the tests as they build exe files that will not run on linux
sed -i 's/TESTS = alltests/TESTS = /' Makefile
rm -rf certs/demo
make
make install_sw

echo "=== done building OPENSSL =="
