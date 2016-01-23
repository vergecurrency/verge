#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/VERGE.ico

convert ../../src/qt/res/icons/VERGE-16.png ../../src/qt/res/icons/VERGE-32.png ../../src/qt/res/icons/VERGE-48.png ${ICON_DST}
