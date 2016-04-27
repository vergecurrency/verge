#!/bin/bash

# using: https://github.com/coinzen/devcoin/blob/master/doc/build-mingw-under_linux.txt as a guide
# Note: This assumes ubuntu:14.04 and we are going to cross-compile the windows binaries

#echo "=== dpkg -l (before)"
#dpkg -l

#sudo apt-get remove -y mingw32 mingw32-binutils mingw32-runtime
sudo apt-get remove -y "^libqt4-.*" "^mingw32.*" > /dev/null

#echo "=== dpkg -l (after removal of some stuffs)"
#dpkg -l

sudo apt-get --yes -qq install software-properties-common > /dev/null
sudo add-apt-repository --yes ppa:ubuntu-sdk-team/ppa > /dev/null
sudo add-apt-repository --yes ppa:bitcoin/bitcoin > /dev/null
sudo apt-get update -qq > /dev/null

sudo apt-get --yes -qq install dpkg-dev git sudo make wget build-essential libtool autotools-dev automake pkg-config git protobuf-compiler autoconf bsdmainutils python curl libssl-dev > /dev/null
# May need these: libdb4.8++-dev libdb4.8-dev 

echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" | sudo tee /etc/apt/sources.list.d/mxeapt.list > /dev/null
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB
sudo apt-get -qq update > /dev/null
sudo apt-get -qq --yes install mxe-i686-w64-mingw32.static-qt mxe-i686-w64-mingw32.static-libodbc++ mxe-i686-w64-mingw32.static-libsigc++ mxe-x86-64-w64-mingw32.static-libodbc++ mxe-x86-64-w64-mingw32.static-libsigc++ > /dev/null

echo "=== dpkg -l (after)"
dpkg -l

export CROSS=i686-w64-mingw32.static-
export CC=${CROSS}gcc
export CXX=${CROSS}g++
export LD=${CROSS}ld
export AR=${CROSS}ar
export RANLIB=${CROSS}ranlib
export PKG_CONFIG=${CROSS}pkg-config

which $CC
which $CXX
which $LD
which $AR
which $RANLIB
which $PKG

export PATH=/usr/lib/mxe/usr/bin/:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH

./building/windows/req_openssl.sh
./building/windows/req_dbd.sh
./building/windows/req_miniupnpc.sh
./building/windows/req_protobuf.sh
./building/windows/req_boost.sh
./building/windows/req_qrencode.sh
./building/windows/req_qt.sh
