#!/bin/bash

set -e

echo "=== Building PROTOBUF now..."
cd /tmp/

# The protobuf from repository can not download Google Mock
# apt-get source libprotobuf-dev
# Remove the "modified" for ubuntu source
# rm -rf /tmp/protobuf-3.0.0
# tar xzf protobuf_3.0.0.orig.tar.gz

# wget https://github.com/google/protobuf/releases/download/v3.0.0/protobuf-cpp-3.0.0.tar.gz
# tar xzf protobuf-cpp-3.0.0.tar.gz

# cd protobuf-3.0.0
# ./autogen.sh --host=i686-w64-mingw32
# ./configure --host=i686-w64-mingw32 --with-protoc=protoc LDFLAGS='-static'

# wget https://github.com/protocolbuffers/protobuf/releases/download/v3.6.1/protobuf-all-3.6.1.tar.gz
# tar xf protobuf-all-3.6.1.tar.gz
# cd protobuf-3.6.1

wget https://github.com/protocolbuffers/protobuf/releases/download/v3.0.0/protobuf-cpp-3.0.0.tar.gz
tar xf protobuf-cpp-3.0.0.tar.gz
cd protobuf-3.0.0

# wget https://github.com/protocolbuffers/protobuf/releases/download/v2.6.1/protobuf-2.6.1.tar.bz2
# tar xzf protobuf-2.6.1.tar.bz2
# cd protobuf-2.6.1

./autogen.sh --host=${CROSS}
./configure --host=${CROSS} --with-protoc=protoc LDFLAGS='-static'

make 
make install
rm -rf protobuf-*

echo "=== Done building PROTOBUF =="
