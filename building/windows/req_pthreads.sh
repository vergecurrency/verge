#!/bin/bash

echo "=== Downloading pthreads now..."

cd /tmp
wget ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.zip
sudo unzip -q pthreads-w32-2-9-1-release.zip
cd /tmp/pthreads.2
sudo make CROSS=i686-w64-mingw32- GC-inlined
sudo ln -s libpthreadGC2.a libpthread.a

echo "=== done pthreads now..."
