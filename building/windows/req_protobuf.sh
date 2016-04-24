#!/bin/bash

# build protobuf 
echo "=== Building PROTOBUF now..."
cd /tmp/
apt-get source libprotobuf-dev > /dev/null
# Remove the "modified" for ubuntu source
rm -rf /tmp/protobuf-2.5.0
tar xzf protobuf_2.5.0.orig.tar.gz
cd protobuf-2.5.0
./autogen.sh --host=i686-w64-mingw32
./configure --host=i686-w64-mingw32 --with-protoc=protoc LDFLAGS='-static'
make 
sudo make install
echo "=== done building PROTOBUF =="
