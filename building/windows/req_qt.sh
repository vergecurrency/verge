#!/bin/bash

# qt4  (see https://wiki.qt.io/MinGW-64-bit )
# considering... https://sourceforge.net/projects/mingwbuilds/files/mingw-sources/4.8.1/src-4.8.1-release-rev5.tar.7z/download
# also considering using the apt-get source ...
echo "=== Building QT now..."
sudo apt-get --yes install unzip libqt4-dev
cd /tmp
wget http://download.qt.io/archive/qt/4.8/4.8.5/qt-everywhere-opensource-src-4.8.5.zip
unzip qt-everywhere-opensource-src-4.8.5.zip
cd /tmp/qt-everywhere-opensource-src-4.8.5
./configure -prefix $PWD/install-release-static -static -confirm-license -release -opensource -no-sql-sqlite -no-audio-backend -nomake examples -nomake tests -no-sse2 -xplatform win32-g++ -device-option CROSS_COMPILE=i686-w64-mingw32- -skip qtactiveqt -skip qtmultimedia -skip qtdoc -skip qtcanvas3d -skip qtactiveqt -skip qtenginio -skip qtlocation -skip qtmultimedia -skip qtserialport -skip qtquick1 -skip qtquickcontrols -skip qtscript -skip qtsensors -skip qtwebkit -skip qtwebsockets -skip qtxmlpatterns
#./configure -prefix $PWD/install-release-static -static -confirm-license -release -opensource -no-sql-sqlite -no-openssl -no-audio-backend -nomake examples -nomake tests -no-sse2 -xplatform win32-g++ -device-option CROSS_COMPILE=i686-w64-mingw32- -skip qtactiveqt -skip qtmultimedia -skip qtdoc -skip qtcanvas3d -skip qtactiveqt -skip qtenginio -skip qtlocation -skip qtmultimedia -skip qtserialport -skip qtquick1 -skip qtquickcontrols -skip qtscript -skip qtsensors -skip qtwebkit -skip qtwebsockets -skip qtxmlpatterns

