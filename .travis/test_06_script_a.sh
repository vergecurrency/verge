#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

TRAVIS_COMMIT_LOG=$(git log --format=fuller -1)
export TRAVIS_COMMIT_LOG

OUTDIR=$BASE_OUTDIR/$TRAVIS_PULL_REQUEST/$TRAVIS_JOB_NUMBER-$HOST

if [[ $HOST = *-mingw32 ]]; then
  VERGE_CONFIG_ALL="--with-gui-qt5 --host=${CROSS} --libdir=$OUTDIR/lib --bindir=$OUTDIR/bin \
    --with-qt-incdir=/usr/local/Qt-5.10.1/include --with-qt-libdir=/usr/local/Qt-5.10.1/lib \ 
    --with-qt-plugindir=/usr/local/Qt-5.10.1/plugins --with-qt-bindir=/usr/local/Qt-5.10.1/bin \ 
    --without-tests --disable-tests --without-bench --disable-bench --without-miniupnpc \
    --disable-asm --includedir=/usr/local/include --includedir=/tmp/zlib-1.2.11 \
    --includedir=/usr/local/Qt-5.10.1/include --includedir=/usr/local/BerkeleyDB.4.8/include \
     --with-boost=/tmp/boost_1_58_0/boost --with-boost-libdir=/tmp/boost_1_58_0/stage/lib"
else
  VERGE_CONFIG_ALL="--disable-dependency-tracking --prefix=$TRAVIS_BUILD_DIR/depends/$HOST --bindir=$OUTDIR/bin --libdir=$OUTDIR/lib"
fi

if [ -z "$NO_DEPENDS" ]; then
  DOCKER_EXEC ccache --max-size=$CCACHE_SIZE
fi

if [[ $HOST = *-mingw32 ]]; then
  BEGIN_FOLD docker-build
    DOCKER_EXEC .travis/lint_06_script_prepare_win.sh
  END_FOLD
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

BEGIN_FOLD build
DOCKER_EXEC make $MAKEJOBS $GOAL || ( echo "Build failure. Verbose build follows." && DOCKER_EXEC make $GOAL V=1 ; false )
END_FOLD

cd ${TRAVIS_BUILD_DIR} || (echo "could not enter travis build dir $TRAVIS_BUILD_DIR"; exit 1)