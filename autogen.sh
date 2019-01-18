#!/bin/bash
# Copyright (c) 2013-2016 The Bitcoin Core developers
# Copyright (c) 2019 The Verge Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

set -e
git submodule update --init --recursive

srcdir="$(dirname $0)"
cd "$srcdir"
if [ -z ${LIBTOOLIZE} ] && GLIBTOOLIZE="`which glibtoolize 2>/dev/null`"; then
  LIBTOOLIZE="${GLIBTOOLIZE}"
  export LIBTOOLIZE
fi
which autoreconf >/dev/null || \
  (echo "configuration failed, please install autoconf first" && exit 1)
autoreconf --install --force --warnings=all

echo "Configure subtrees (secp256k1 and tor)"
(cd "${srcdir}/src/secp256k1" && ./autogen.sh)
(cd "${srcdir}/src/tor" && ./autogen.sh)
