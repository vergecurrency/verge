#!/bin/bash

# build protobuf 
echo "=== Building PROTOBUF now..."
cd /tmp/
apt-get source libprotobuf-dev > /dev/null
# Remove the "modified" for ubuntu source
rm -rf /tmp/protobuf-2.5.0
tar xzf protobuf_2.5.0.orig.tar.gz
cd protobuf-2.5.0
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH ./autogen.sh --host=i686-w64-mingw32
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH ./configure --host=i686-w64-mingw32 --with-protoc=protoc LDFLAGS='-static'
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH sudo make CROSS=i686-w64-mingw32.static-
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH sudo make CROSS=i686-w64-mingw32.static- install
echo "=== done building PROTOBUF =="
