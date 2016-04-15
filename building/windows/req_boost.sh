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
echo "using gcc : 4.9.2 : i686-w64-mingw32-g++ : <rc>i686-w64-mingw32-windres <archiver>i686-w64-mingw32-ar ;" > user-config.jam
./b2 --user-config=user-config.jam toolset=gcc-mingw binary-format=pe target-os=windows release --prefix=/usr/i686-w64-mingw32/local --without-python --without-wave --without-context --without-coroutine --without-mpi --without-graph --without-graph_parallel -sNO_BZIP2=1 threadapi=win32 > /tmp/boost_build.log 2>&1

