#!/bin/bash

# using: https://github.com/coinzen/devcoin/blob/master/doc/build-mingw-under_linux.txt as a guide
# Note: This assumes ubuntu:14.04 and we are going to cross-compile the windows binaries

#echo "=== dpkg -l (before)"
#dpkg -l

#sudo apt-get remove -y mingw32 mingw32-binutils mingw32-runtime
apt-get remove -y "^libqt4-.*" "^mingw32.*"

#echo "=== dpkg -l (after removal of some stuffs)"
#dpkg -l

apt-get update
apt-get --yes install software-properties-common
# add-apt-repository --yes ppa:ubuntu-sdk-team/ppa
add-apt-repository --yes ppa:bitcoin/bitcoin
apt-get update

apt-get --yes install dpkg-dev git sudo make wget build-essential libtool autotools-dev automake pkg-config git protobuf-compiler autoconf bsdmainutils python curl libssl-dev vim
# May need these: libdb4.8++-dev libdb4.8-dev 

# The server pkg.mxe.cc is down now
# echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" | sudo tee /etc/apt/sources.list.d/mxeapt.list
# apt-key adv --keyserver keyserver.ubuntu.com --recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB

# This repository contains only windows thread model, but we need posix thread model
# add-apt-repository 'deb [arch=amd64] https://mirror.mxe.cc/repos/apt xenial main'
# apt-key adv --keyserver keyserver.ubuntu.com --recv-keys C6BF758A33A3A276

# apt-get update
# apt-get --yes install mxe-i686-w64-mingw32.static-qt mxe-i686-w64-mingw32.static-libodbc++ mxe-i686-w64-mingw32.static-libsigc++ mxe-x86-64-w64-mingw32.static-libodbc++ mxe-x86-64-w64-mingw32.static-libsigc++

apt install g++-mingw-w64-i686 mingw-w64-i686-dev
# Set the default mingw32 g++ compiler option to posix.
update-alternatives --config i686-w64-mingw32-g++

echo "=== dpkg -l (after)"
dpkg -l

export PATH=/usr/lib/mxe/usr/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH

#export CROSS=i686-w64-mingw32.static-
export CROSS=i686-w64-mingw32-
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
which $PKG_CONFIG

./building/windows/req_openssl.sh
./building/windows/req_dbd.sh
./building/windows/req_miniupnpc.sh
./building/windows/req_protobuf.sh
./building/windows/req_zlib.sh
./building/windows/req_boost.sh
./building/windows/req_qrencode.sh

# Build libevent
cd /tmp/

wget https://github.com/libevent/libevent/releases/download/release-2.1.8-stable/libevent-2.1.8-stable.tar.gz

tar xf libevent-2.1.8-stable.tar.gz
cd libevent-2.1.8-stable
./configure --host=i686-w64-mingw32 --enable-static --disable-shared --disable-openssl
make
make install

./building/windows/req_qt.sh