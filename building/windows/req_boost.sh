#!/bin/bash

# boost
echo "=== Building BOOST now..."
cd /tmp
wget wget 'http://sourceforge.net/projects/boost/files/boost/1.60.0/boost_1_60_0.tar.bz2/download' -O boost_1_60_0.tar.bz2
tar jxf boost_1_60_0.tar.bz2
cd /tmp/boost_1_60_0

# (if troubleshooting and want to 'make clean all')
# bjam --clean 

./bootstrap.sh --without-icu --with-libraries=thread --with-libraries=system --with-libraries=test
echo "using gcc : 4.9.2 : i686-w64-mingw32-g++ : <rc>i686-w64-mingw32-windres <archiver>i686-w64-mingw32-ar ;" > user-config.jam
sed -i 's/ -Wunused-local-typedefs//' ./libs/core/test/Jamfile.v2
sed -i 's/40700/10/' ./boost/tuple/detail/tuple_basic.hpp
sed -i 's/#.*//' libs/context/src/unsupported.cpp
./b2 -j10 --user-config=user-config.jam toolset=gcc-mingw address-model=32 binary-format=pe target-os=windows release --prefix=/usr/i686-w64-mingw32/local --without-python --without-wave --without-context --without-coroutine --without-mpi --without-test --without-graph --without-graph_parallel -sNO_BZIP2=1
sed -i 's/ -Wunused-local-typedefs//' ./libs/core/test/Jamfile.v2
./b2 -j10 --user-config=user-config.jam toolset=gcc-mingw address-model=32 binary-format=pe target-os=windows release --prefix=/usr/i686-w64-mingw32/local --without-python --without-wave --without-context --without-coroutine --without-mpi --without-test --without-graph --without-graph_parallel -sNO_BZIP2=1

