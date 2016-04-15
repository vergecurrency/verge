#!/bin/bash

echo "=== Downloading pthreads now..."

cd /tmp
wget ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.zip
unzip pthreads-w32-2-9-1-release.zip

echo "=== done pthreads now..."
