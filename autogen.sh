#!/bin/sh
set -e
pushd tor && git checkout -f . && popd
git submodule update --init
srcdir="$(dirname $0)"
cd "$srcdir"
autoreconf --install --force


