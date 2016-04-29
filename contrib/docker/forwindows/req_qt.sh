#!/bin/bash

echo "=== Building QT now..."
cd /tmp/
wget -q http://download.qt.io/official_releases/qt/5.4/5.4.2/single/qt-everywhere-opensource-src-5.4.2.tar.gz
tar xzf qt-everywhere-opensource-src-5.4.2.tar.gz
cd /tmp/qt-everywhere-opensource-src-5.4.2

./configure -static -confirm-license -release -opensource -no-sql-sqlite -no-audio-backend -nomake examples -nomake tests -no-sse2 -xplatform win32-g++ -device-option CROSS_COMPILE=i686-w64-mingw32.static- -skip qtactiveqt -skip qtmultimedia -skip qtdoc -skip qtactiveqt -skip qtenginio -skip qtlocation -skip qtmultimedia -skip qtserialport -skip qtquickcontrols -skip qtscript -skip qtsensors -skip qtxmlpatterns

make -j 4
sudo make install

echo "=== done building QT =="
