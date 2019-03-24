#!/bin/bash

set -e

echo "=== Building BOOST now..."

cd /tmp
wget 'http://sourceforge.net/projects/boost/files/boost/1.58.0/boost_1_58_0.tar.bz2/download' -O boost_1_58_0.tar.bz2
tar jxf boost_1_58_0.tar.bz2
cd /tmp/boost_1_58_0

# (if troubleshooting and want to 'make clean all')
# bjam --clean 

./bootstrap.sh --without-icu  

echo "using gcc : 7.3.0 : ${CROSS}-g++ : <rc>${CROSS}-windres <archiver>${CROSS}-ar ;" > user-config.jam

./b2 -d 0 --user-config=user-config.jam toolset=gcc-mingw binary-format=pe target-os=windows release --without-python --without-wave --without-context --without-coroutine --without-mpi --without-graph --without-graph_parallel -sNO_BZIP2=1 threadapi=win32 threading=multi variant=release link=static runtime-link=static

echo "=== Done building BOOST =="
