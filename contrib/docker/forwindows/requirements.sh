#!/bin/bash
set -e

# using: https://github.com/coinzen/devcoin/blob/master/doc/build-mingw-under_linux.txt as a guide

echo "=== dpkg -l (before)"
dpkg -l

# We should not delete these packages at Ubuntu 18.04
# apt-get remove --yes "^libqt4-.*" "^mingw32.*"

# echo "=== dpkg -l (after removal of some stuffs)"
# dpkg -l

apt-get update
apt-get --yes install software-properties-common
# add-apt-repository --yes ppa:ubuntu-sdk-team/ppa
add-apt-repository --yes ppa:bitcoin/bitcoin
apt-get update

apt-get --yes install dpkg-dev git make wget build-essential libtool autotools-dev automake pkg-config git protobuf-compiler autoconf bsdmainutils python curl libssl-dev vim
# May need these: libdb4.8++-dev libdb4.8-dev 

# The server pkg.mxe.cc is down now
# echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" | sudo tee /etc/apt/sources.list.d/mxeapt.list
# apt-key adv --keyserver keyserver.ubuntu.com --recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB

# This repository contains only windows thread model, but we need posix thread model
# add-apt-repository 'deb [arch=amd64] https://mirror.mxe.cc/repos/apt xenial main'
# apt-key adv --keyserver keyserver.ubuntu.com --recv-keys C6BF758A33A3A276

# apt-get update
# apt-get --yes install mxe-i686-w64-mingw32.static-qt mxe-i686-w64-mingw32.static-libodbc++ mxe-i686-w64-mingw32.static-libsigc++ mxe-x86-64-w64-mingw32.static-libodbc++ mxe-x86-64-w64-mingw32.static-libsigc++

# Now we do not use mxe toolchain
# export PATH=/usr/lib/mxe/usr/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:$PATH

if [ "$TARGET_PLATFORM" == "win64" ]
then
  echo "The target platform is win64"

  apt-get install --yes g++-mingw-w64-x86-64 mingw-w64-x86-64-dev

  # Set the default mingw32 g++ compiler option to posix.
  update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
  update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
elif [ "$TARGET_PLATFORM" == "win32" ]
then
  echo "The target platform is win32"

  apt-get install --yes g++-mingw-w64-i686 mingw-w64-i686-dev

  # Set the default mingw32 g++ compiler option to posix.
  update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix
  update-alternatives --set i686-w64-mingw32-gcc /usr/bin/i686-w64-mingw32-gcc-posix
else
  echo "You should setup TARGET_PLATFORM environment variable. It can be win64 or win32"
  exit 1
fi

echo "=== dpkg -l (after)"
dpkg -l

# We use x86_64-w64-mingw32 or i686-w64-mingw32 toolchains now, not i686-w64-mingw32.static
# export CROSS=i686-w64-mingw32.static

# Use it if you want to use x64 toolchain
# export CROSS=x86_64-w64-mingw32

# Use it if you want to use x32 toolchain
# export CROSS=i686-w64-mingw32

# Uncomment this if you run the script without docker, or if you want to play with toolchains
# export CC=${CROSS}-gcc
# export CXX=${CROSS}-g++
# export LD=${CROSS}-ld
# export AR=${CROSS}-ar
# export RANLIB=${CROSS}-ranlib
# export PKG_CONFIG=${CROSS}-pkg-config

echo "CC: `command -v $CC`"
echo "CXX: `command -v $CXX`"
echo "LD: `command -v $LD`"
echo "AR: `command -v $AR`"
echo "RANLIB: `command -v $RANLIB`"
echo "PKG_CONFIG: `command -v $PKG_CONFIG`"

./req_openssl.sh
./req_dbd.sh
# ./req_miniupnpc.sh
./req_protobuf.sh
./req_zlib.sh
./req_boost.sh
./req_qrencode.sh
./req_libevent.sh
./req_qt.sh