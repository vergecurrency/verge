#!/bin/bash

set -e

# Build the windows exe

export LIBS='-L/usr/local/Qt-5.10.1/lib -L/usr/local/Qt-5.10.1/plugins/platforms -L/usr/local/lib -lqminimal -lqwindows -lqoffscreen -L/usr/local/Qt-5.10.1/plugins/imageformats -L/tmp/zlib-1.2.11 -lqjpeg -lqicns -lqico -lqsvg -lqwebp -lqgif -lqwbmp -lqtharfbuzz -lqtpcre2 -lQt5Core -lQt5Network -lQt5Widgets -lQt5WinExtras -lQt5AccessibilitySupport -lQt5OpenGL -lQt5OpenGLExtensions -lQt5PlatformCompositorSupport -lQt5ThemeSupport -lQt5Svg -lQt5Gui -lQt5FontDatabaseSupport -lqtfreetype -lQt5EventDispatcherSupport -lqtlibpng -lprotobuf -levent -lqrencode -lssl -lcrypto -lcrypt32 -liphlpapi -lshlwapi -lmswsock -ladvapi32 -lrpcrt4 -luuid -loleaut32 -lole32 -lcomctl32 -lshell32 -lwinmm -lwinspool -lcomdlg32 -lgdi32 -luser32 -lkernel32 -lmingwthrd -lz -lws2_32 -pthread -lshlwapi -lnetapi32 -luserenv -lversion -luxtheme -ldwmapi -limm32'

export LDFLAGS='-L/usr/local/lib -L/usr/local/BerkeleyDB.4.8/lib -L/tmp/boost_1_58_0/stage/lib -L/tmp/miniupnpc-1.9.20160209 -L/tmp/zlib-1.2.11 -lssl -lcrypto -lz -static -levent' 

export CFLAGS='-std=c++11 -I/usr/local/Qt-5.10.1/include/ -I/usr/local/BerkeleyDB.4.8/include -I/tmp/boost_1_58_0/boost -I/tmp/boost_1_58_0 -I/tmp -I/usr/local/include -I/tmp/zlib-1.2.11'

export CXXFLAGS=$CFLAGS
export CPPFLAGS=$CFLAGS
export BOOST_ROOT=/tmp/boost_1_58_0

./autogen.sh --host=${CROSS}-

mkdir build && cd build
../configure --with-gui-qt5 --host=${CROSS} --with-qt-incdir=/usr/local/Qt-5.10.1/include --with-qt-libdir=/usr/local/Qt-5.10.1/lib --with-qt-plugindir=/usr/local/Qt-5.10.1/plugins --with-qt-bindir=/usr/local/Qt-5.10.1/bin --without-tests --disable-tests  --without-bench --disable-bench --without-miniupnpc --disable-asm --includedir=/usr/local/include --includedir=/tmp/zlib-1.2.11 --includedir=/usr/local/Qt-5.10.1/include --includedir=/usr/local/BerkeleyDB.4.8/include --with-boost=/tmp/boost_1_58_0/boost --with-boost-libdir=/tmp/boost_1_58_0/stage/lib

make distdir VERSION=$HOST

make
make deploy

# For debugging purposes
# cat src/test-suite.log

echo "Done"
