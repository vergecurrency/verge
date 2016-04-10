#!/bin/bash

set -xue

echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" \
    | sudo tee /etc/apt/sources.list.d/mxeapt.list
sudo apt-key adv --keyserver x-hkp://keys.gnupg.net \
    --recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB

sudo apt-get update

sudo apt-get --yes install upx-ucl

if [ "$VERGE_PLATFORM" = "windows32" ]; then
    MXE_TARGET=i686-w64-mingw32.static
fi

if [ "$VERGE_PLATFORM" = "windows64" ]; then
    MXE_TARGET=x86-64-w64-mingw32.static
fi

MXE2_TARGET=$(echo "$MXE_TARGET" | sed 's/_/-/g')
sudo apt-get --yes install \
    mxe-${MXE2_TARGET}-qt

# MXE doesn't have 64bit NSIS
#sudo apt-get --yes install \
    #mxe-i686-w64-mingw32.static-nsis

set +xue
