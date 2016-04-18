#!/bin/bash

# boost
echo "=== Building BOOST now..."
cd /tmp
wget wget 'http://sourceforge.net/projects/boost/files/boost/1.55.0/boost_1_55_0.tar.bz2/download' -O boost_1_55_0.tar.bz2
tar jxf boost_1_55_0.tar.bz2
cd /tmp/boost_1_55_0

# (if troubleshooting and want to 'make clean all')
# bjam --clean 

./bootstrap.sh --without-icu  
echo "using gcc : 4.9.2 : i686-w64-mingw32.static-g++ : <rc>i686-w64-mingw32.static-windres <archiver>i686-w64-mingw32.static-ar ;" > user-config.jam
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH sudo ./b2 -d 0 --user-config=user-config.jam toolset=gcc-mingw binary-format=pe target-os=windows release --without-python --without-wave --without-context --without-coroutine --without-mpi --without-graph --without-graph_parallel -sNO_BZIP2=1 threadapi=win32 threading=multi variant=release link=static runtime-link=static install
echo "=== done building BOOST =="
