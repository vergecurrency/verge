#!/bin/bash

echo "=== Building QT now..."
cd /tmp/
wget http://download.qt.io/official_releases/qt/5.6/5.6.0/single/qt-everywhere-opensource-src-5.6.0.tar.gz
tar xzf qt-everywhere-opensource-src-5.6.0.tar.gz
cd /tmp/qt-everywhere-opensource-src-5.6.0

# "fix" for fatal error: ft2build.h: No such file or directory
#ln -s /usr/lib/mxe/usr/i686-w64-mingw32.static/include/freetype2/ft2build.h /tmp/qt-everywhere-opensource-src-5.6.0/qtbase/include/QtCore/
#ln -s /usr/lib/mxe/usr/i686-w64-mingw32.static/include/freetype2/freetype /tmp/qt-everywhere-opensource-src-5.6.0/qtbase/include/QtCore/
#ln -s /usr/lib/mxe/usr/i686-w64-mingw32.static/lib/libfreetype.a /tmp/qt-everywhere-opensource-src-5.6.0/qtbase/lib/
#ln -s /usr/lib/mxe/usr/i686-w64-mingw32.static/lib/libfreetype.a /tmp/qt-everywhere-opensource-src-5.6.0/qtbase/lib/
#ln -s /usr/lib/mxe/usr/i686-w64-mingw32.static/lib/libfreetype.la /tmp/qt-everywhere-opensource-src-5.6.0/qtbase/lib/
# Note: Could not get qt5 to compile with '-system-freetype -fontconfig' Not sure if it is going to cause problems yet.

PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH CPPFLAGS="/usr/lib/mxe/usr/i686-w64-mingw32.shared/include/freetype2" ./configure -static -confirm-license -release -opensource -no-sql-sqlite -no-audio-backend -nomake examples -nomake tests -no-sse2 -xplatform win32-g++ -device-option CROSS_COMPILE=i686-w64-mingw32.static- -skip qtactiveqt -skip qtmultimedia -skip qtdoc -skip qtcanvas3d -skip qtactiveqt -skip qtenginio -skip qtlocation -skip qtmultimedia -skip qtserialport -skip qtquickcontrols -skip qtscript -skip qtsensors -skip qtwebsockets -skip qtxmlpatterns 

PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH make -j 4
PATH=/usr/lib/mxe/usr/bin/:/usr/bin:$PATH make install

echo "=== done building QT =="
