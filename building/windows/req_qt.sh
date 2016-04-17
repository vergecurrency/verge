#!/bin/bash

echo "=== Building QT now..."
cd /tmp/

#sudo apt-get --yes install unzip libqt5-dev > /dev/null
wget http://download.qt.io/official_releases/qt/5.5/5.5.0/single/qt-everywhere-opensource-src-5.5.0.tar.gz
tar xzf qt-everywhere-opensource-src-5.5.0.tar.gz
cd /tmp/qt-everywhere-opensource-src-5.5.0
PATH=/usr/lib/mxe/usr/bin/:$PATH ./configure -prefix $PWD/install-release-static -static -confirm-license -release -opensource -no-sql-sqlite -no-audio-backend -nomake examples -nomake tests -no-sse2 -xplatform win32-g++ -device-option CROSS_COMPILE=i686-w64-mingw32.static- -skip qtactiveqt -skip qtmultimedia -skip qtdoc -skip qtcanvas3d -skip qtactiveqt -skip qtenginio -skip qtlocation -skip qtmultimedia -skip qtserialport -skip qtquick1 -skip qtquickcontrols -skip qtscript -skip qtsensors -skip qtwebkit -skip qtwebsockets -skip qtxmlpatterns -system-freetype  -fontconfig
PATH=/usr/lib/mxe/usr/bin/:$PATH make

echo "=== done building QT =="
