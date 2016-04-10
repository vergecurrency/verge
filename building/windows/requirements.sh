#!/bin/bash

set -xue

echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" \
    | sudo tee /etc/apt/sources.list.d/mxeapt.list
sudo apt-key adv --keyserver x-hkp://keys.gnupg.net \
    --recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB

sudo apt-get update

sudo apt-get --yes install upx-ucl

sudo apt-get --yes install p7zip-full autoconf automake autopoint bash bison bzip2 cmake flex gettext git g++ gperf intltool libffi-dev libtool libltdl-dev libssl-dev libxml-parser-perl make openssl patch perl pkg-config python ruby scons sed unzip wget xz-utils

sudo apt-get --yes install g++-multilib libc6-dev-i386

if [ "$VERGE_PLATFORM" = "windows32" ]; then
    MXE_TARGET=i686-w64-mingw32.static
fi

if [ "$VERGE_PLATFORM" = "windows64" ]; then
    MXE_TARGET=x86-64-w64-mingw32.static
fi

MXE2_TARGET=$(echo "$MXE_TARGET" | sed 's/_/-/g')
sudo apt-get --yes install \
    mxe-${MXE2_TARGET}-qt

# build db
cd /tmp
wget http://download.oracle.com/otn/berkeley-db/db-5.3.28.tar.gz
tar xzf db-5.3.28.tar.gz
cd /tmp/db-5.3.28
touch compile-db.sh
chmod ugo+x compile-db.sh
sed -i "s/WinIoCtl.h/winioctl.h/g" src/dbinc/win_db.h
mkdir build_mxe
cd build_mxe

CC=${MXE_TARGET} \
CXX=${MXE_TARGET} \
../dist/configure \
	--disable-replication \
	--enable-mingw \
	--enable-cxx \
	--host x86 \
	--prefix=${MXE_TARGET}

make
make install

set +xue
