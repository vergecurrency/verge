#!/bin/bash

# using: https://github.com/coinzen/devcoin/blob/master/doc/build-mingw-under_linux.txt as a guide
# Note: This assumes ubuntu:14.04 and we are going to cross-compile the windows binaries

sudo apt-get --yes install software-properties-common
sudo add-apt-repository --yes ppa:ubuntu-sdk-team/ppa
sudo add-apt-repository --yes ppa:bitcoin/bitcoin
sudo apt-get update -qq

# add the cross compiling stuffs
sudo apt-get --yes install dpkg-dev git sudo make mingw-w64 mingw-w64-common wget g++-mingw-w64 binutils-mingw-w64-x86-64 g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 mingw-w64-tools mingw-w64-x86-64-dev zip

# base requirements for building wallet (pulled from linux)
sudo apt-get --yes install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils git libboost-all-dev libminiupnpc-dev libprotobuf-dev protobuf-compiler libqrencode-dev protobuf-compiler autoconf bsdmainutils
# TODO: (need or want?) libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools 

./building/${VERGE_PLATFORM}/req_openssl.sh
./building/${VERGE_PLATFORM}/req_pthreads.sh
./building/${VERGE_PLATFORM}/req_dbd.sh
./building/${VERGE_PLATFORM}/req_miniupnpc.sh
./building/${VERGE_PLATFORM}/req_protobuf.sh
./building/${VERGE_PLATFORM}/req_boost.sh
# TODO: need? ./building/${VERGE_PLATFORM}/req_libevent.sh
./building/${VERGE_PLATFORM}/req_qt.sh
