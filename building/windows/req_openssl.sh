#!/bin/bash

# build openssl
echo "=== Building OPENSSL now..."
cd /tmp/
sudo apt-get source openssl > /dev/null
openssl_version=`dpkg -s openssl | grep Version | sed 's/Version: \(.*\)-.*/\1/'`
cd /tmp/openssl-${openssl_version}
sudo ./Configure mingw64 no-zlib no-asm no-shared no-dso no-asm 
# skip the tests as they build exe files that will not run on linux
sudo sed -i 's/TESTS = alltests/TESTS = /' Makefile
sudo rm -rf certs/demo
make

echo "=== done building OPENSSL =="
