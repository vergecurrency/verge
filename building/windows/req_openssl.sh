#!/bin/bash

# build openssl
echo "=== Building OPENSSL now..."
cd /tmp/
sudo apt-get source openssl > /dev/null
openssl_version=`dpkg -s openssl | grep Version | sed 's/Version: \(.*\)-.*/\1/'`
cd /tmp/openssl-${openssl_version}
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH CROSS_COMPILE="i686-w64-mingw32.static-" sudo ./Configure mingw64 no-zlib no-asm no-shared no-dso no-asm 
# skip the tests as they build exe files that will not run on linux
sudo sed -i 's/TESTS = alltests/TESTS = /' Makefile
sudo rm -rf certs/demo
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH make library_build
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH sudo make install_lib install_include

echo "=== done building OPENSSL =="
