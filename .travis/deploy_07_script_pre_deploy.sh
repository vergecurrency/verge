#!/usr/bin/env bash
#
# Copyright (c) 2019 The Verge Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

if [[ $SHOULD_DEPLOY = 1 ]]; then
    mv $TRAVIS_BUILD_DIR/build/verge-$HOST/src/qt/verge-qt $TRAVIS_BUILD_DIR/verge-qt-$HOST;
    mv $TRAVIS_BUILD_DIR/build/verge-$HOST/src/qt/verge-qt $TRAVIS_BUILD_DIR/verge-qt-$HOST; 
fi