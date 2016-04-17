#!/bin/bash

echo "deb http://pkg.mxe.cc/repos/apt/debian wheezy main" | sudo tee /etc/apt/sources.list.d/mxeapt.list > /dev/null
sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys D43A795B73B16ABE9643FE1AFD8FFF16DB45C6AB
sudo apt-get -qq update > /dev/null
sudo apt-get -qq --yes install mxe-i686-w64-mingw32.static-qt mxe-i686-w64-mingw32.shared-libsigc++ mxe-i686-w64-mingw32.static-libodbc++ mxe-i686-w64-mingw32.static-libsigc++ mxe-x86-64-w64-mingw32.shared-libsigc++ mxe-x86-64-w64-mingw32.static-libodbc++ mxe-x86-64-w64-mingw32.static-libsigc++ > /dev/null

