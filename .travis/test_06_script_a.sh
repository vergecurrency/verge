#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

TRAVIS_COMMIT_LOG=$(git log --format=fuller -1)
export TRAVIS_COMMIT_LOG

OUTDIR=$BASE_OUTDIR/$TRAVIS_PULL_REQUEST/$TRAVIS_JOB_NUMBER-$HOST
VERGE_CONFIG_ALL="--disable-dependency-tracking --prefix=$TRAVIS_BUILD_DIR/depends/$HOST --bindir=$OUTDIR/bin --libdir=$OUTDIR/lib"
if [ -z "$NO_DEPENDS" ]; then
  DOCKER_EXEC ccache --max-size=$CCACHE_SIZE
fi

BEGIN_FOLD autogen
if [ -n "$CONFIG_SHELL" ]; then
  DOCKER_EXEC "$CONFIG_SHELL" -c "./autogen.sh"
else
  DOCKER_EXEC ./autogen.sh
fi
END_FOLD

mkdir build
cd build || (echo "could not enter build directory"; exit 1)

BEGIN_FOLD configure
DOCKER_EXEC ../configure $VERGE_CONFIG_ALL $VERGE_CONFIG || ( cat config.log && false)
END_FOLD

BEGIN_FOLD distdir
DOCKER_EXEC make distdir VERSION=$HOST
END_FOLD

cd "verge-$HOST" || (echo "could not enter distdir verge-$HOST"; exit 1)

BEGIN_FOLD configure
DOCKER_EXEC ./configure $VERGE_CONFIG_ALL $VERGE_CONFIG || ( cat config.log && false)
END_FOLD

cd "src/tor/" || (echo "could not enter distdir src/tor"; exit 1)

BEGIN_FOLD remove-old-config-tor
DOCKER_EXEC sudo rm -f config.*
END_FOLD

BEGIN_FOLD configure-tor
DOCKER_EXEC sudo ./configure --disable-shared --with-pic --with-bignum=no --enable-module-recovery --disable-jni --disable-unittests --disable-system-torrc --disable-systemd --disable-lzma --disable-zstd --disable-asciidoc
END_FOLD

cd "../../" || (echo "could not return to verge-$HOST"; exit 1)

set -o errtrace
trap 'DOCKER_EXEC "cat ${TRAVIS_BUILD_DIR}/sanitizer-output/* 2> /dev/null"' ERR

BEGIN_FOLD build
DOCKER_EXEC make $MAKEJOBS $GOAL || ( echo "Build failure. Verbose build follows." && DOCKER_EXEC make $GOAL V=1 ; false )
END_FOLD

cd ${TRAVIS_BUILD_DIR} || (echo "could not enter travis build dir $TRAVIS_BUILD_DIR"; exit 1)
