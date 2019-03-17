#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

travis_retry docker pull "$DOCKER_NAME_TAG"
mkdir -p "${TRAVIS_BUILD_DIR}/sanitizer-output/"
export ASAN_OPTIONS=""
export LSAN_OPTIONS="suppressions=${TRAVIS_BUILD_DIR}/test/sanitizer_suppressions/lsan"
export TSAN_OPTIONS="suppressions=${TRAVIS_BUILD_DIR}/test/sanitizer_suppressions/tsan:log_path=${TRAVIS_BUILD_DIR}/sanitizer-output/tsan"
export UBSAN_OPTIONS="suppressions=${TRAVIS_BUILD_DIR}/test/sanitizer_suppressions/ubsan:print_stacktrace=1:halt_on_error=1"
env | grep -E '^(VERGE_CONFIG|CCACHE_|WINEDEBUG|LC_ALL|BOOST_TEST_RANDOM|CONFIG_SHELL|(ASAN|LSAN|TSAN|UBSAN)_OPTIONS)' | tee /tmp/env
if [[ $HOST = *-mingw32 ]]; then
  DOCKER_ADMIN="--cap-add SYS_ADMIN"
elif [[ $VERGE_CONFIG = *--with-sanitizers=*address* ]]; then # If ran with (ASan + LSan), Docker needs access to ptrace (https://github.com/google/sanitizers/issues/764)
  DOCKER_ADMIN="--cap-add SYS_PTRACE"
fi

if [[ $HOST = *86-w64-mingw32 ]]; then
  DOCKER_ID=$(docker run $DOCKER_ADMIN -idt --mount type=bind,src=$TRAVIS_BUILD_DIR,dst=$TRAVIS_BUILD_DIR --mount type=bind,src=$CCACHE_DIR,dst=$CCACHE_DIR -w $TRAVIS_BUILD_DIR --env-file /tmp/env marpme/verge-win32-base:v1)
elif [[ $HOST = *64-w64-mingw32 ]]; then
  DOCKER_ID=$(docker run $DOCKER_ADMIN -idt --mount type=bind,src=$TRAVIS_BUILD_DIR,dst=$TRAVIS_BUILD_DIR --mount type=bind,src=$CCACHE_DIR,dst=$CCACHE_DIR -w $TRAVIS_BUILD_DIR --env-file /tmp/env marpme/verge-win64-base:v1)
else
  DOCKER_ID=$(docker run $DOCKER_ADMIN -idt --mount type=bind,src=$TRAVIS_BUILD_DIR,dst=$TRAVIS_BUILD_DIR --mount type=bind,src=$CCACHE_DIR,dst=$CCACHE_DIR -w $TRAVIS_BUILD_DIR --env-file /tmp/env $DOCKER_NAME_TAG)
fi

DOCKER_EXEC () {
  if [[ $HOST = *-mingw32 ]]; then
    docker exec $DOCKER_ID bash -c "$*"
  else
    docker exec $DOCKER_ID bash -c "cd $PWD && $*"
  fi
}

if [ -n "$DPKG_ADD_ARCH" ]; then
  DOCKER_EXEC dpkg --add-architecture "$DPKG_ADD_ARCH"
fi

travis_retry DOCKER_EXEC apt-get update
travis_retry DOCKER_EXEC apt-get install --no-install-recommends --no-upgrade -qq $PACKAGES $DOCKER_PACKAGES

