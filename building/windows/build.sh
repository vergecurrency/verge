#!/bin/bash


set -xue

MXE_DIR=/usr/lib/mxe

if [ "$VERGE_PLATFORM" = "windows32" ]; then
    MXE_TARGET=i686-w64-mingw32.static
fi

if [ "$VERGE_PLATFORM" = "windows64" ]; then
    MXE_TARGET=x86_64-w64-mingw32.static
fi

echo "TODO: build for windows goes here..."
#${MXE_DIR}/usr/bin/${MXE_TARGET}-cmake . -Bbuild-dir
#cmake --build build-dir --config Release

set +xue
