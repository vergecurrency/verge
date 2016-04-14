#!/bin/bash

# Note: This assumes ubuntu:14.04 and we are going to cross-compile the windows binaries

sudo apt-get --yes install software-properties-common
sudo add-apt-repository --yes ppa:ubuntu-sdk-team/ppa
sudo add-apt-repository --yes ppa:bitcoin/bitcoin
sudo apt-get update -qq

pwd

# using: https://github.com/coinzen/devcoin/blob/master/doc/build-mingw-under_linux.txt as a guide

#sudo apt-get --yes upgrade 
#sudo apt-get --yes update
sudo apt-get --yes install dpkg-dev git sudo make mingw-w64 mingw-w64-common wget g++-mingw-w64 binutils-mingw-w64-x86-64 g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 mingw-w64-tools mingw-w64-x86-64-dev

sudo apt-get --yes install software-properties-common
sudo add-apt-repository --yes ppa:ubuntu-sdk-team/ppa
sudo add-apt-repository --yes ppa:bitcoin/bitcoin
#sudo apt-get --yes update -qq

./building/${VERGE_PLATFORM}/req_openssl.sh
./building/${VERGE_PLATFORM}/req_dbd.sh
./building/${VERGE_PLATFORM}/req_miniupnpc.sh
./building/${VERGE_PLATFORM}/req_boost.sh
./building/${VERGE_PLATFORM}/req_qt.sh

# build wallet
#cd /tmp
#git clone https://github.com/vergecurrency/verge 
#cd verge

# TODO: more work here...

