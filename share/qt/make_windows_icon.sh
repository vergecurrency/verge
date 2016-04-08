#!/bin/bash
# create multiresolution windows icon
ICON_DST=../../src/qt/res/icons/verge.ico

convert ../../src/qt/res/icons/verge-16.png ../../src/qt/res/icons/verge-32.png ../../src/qt/res/icons/verge-48.png ${ICON_DST}
