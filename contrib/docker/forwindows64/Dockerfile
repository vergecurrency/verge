FROM ubuntu:18.04

MAINTAINER Mike Kinney <mike.kinney@gmail.com>

ADD ./requirements.sh /tmp/
ADD ./req_boost.sh /building/windows/req_boost.sh
ADD ./req_dbd.sh /building/windows/req_dbd.sh
ADD ./req_miniupnpc.sh /building/windows/req_miniupnpc.sh
ADD ./req_openssl.sh /building/windows/req_openssl.sh
ADD ./req_protobuf.sh /building/windows/req_protobuf.sh
ADD ./req_zlib.sh /building/windows/req_zlib.sh
ADD ./req_qrencode.sh /building/windows/req_qrencode.sh
ADD ./req_qt.sh /building/windows/req_qt.sh

ENV DEBIAN_FRONTEND noninteractive

ENV CROSS i686-w64-mingw32.static- 
ENV CC i686-w64-mingw32.static-gcc 
ENV CXX i686-w64-mingw32.static-g++ 
ENV LD i686-w64-mingw32.static-ld 
ENV AR i686-w64-mingw32.static-ar 
ENV RANLIB i686-w64-mingw32.static-gcc-ranlib 
ENV PKG_CONFIG i686-w64-mingw32.static-pkg-config 
ENV PATH /usr/lib/mxe/usr/bin/:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin 

RUN /tmp/requirements.sh
RUN ./building/windows/req_openssl.sh
RUN ./building/windows/req_dbd.sh
RUN ./building/windows/req_miniupnpc.sh
RUN ./building/windows/req_protobuf.sh
RUN ./building/windows/req_zlib.sh
RUN ./building/windows/req_boost.sh
RUN ./building/windows/req_qrencode.sh
RUN ./building/windows/req_qt.sh

WORKDIR /tmp
ENV LIBS '-L/usr/local/Qt-5.10.1/lib -L/usr/local/Qt-5.10.1/plugins/platforms -L/usr/local/lib -lqminimal -lqwindows -lqoffscreen -L/usr/local/Qt-5.10.1/plugins/imageformats -L/tmp/zlib-1.2.11 -lqjpeg -lqicns -lqico -lqsvg -lqwebp -lqgif -lqwbmp -lqtharfbuzz -lqtpcre2 -lQt5Core -lQt5Network -lQt5Widgets -lQt5WinExtras -lQt5AccessibilitySupport -lQt5OpenGL -lQt5OpenGLExtensions -lQt5PlatformCompositorSupport -lQt5ThemeSupport -lQt5Svg -lQt5Gui -lQt5FontDatabaseSupport -lqtfreetype -lQt5EventDispatcherSupport -lqtlibpng -lprotobuf -levent -lqrencode -lssl -lcrypto -lcrypt32 -liphlpapi -lshlwapi -lmswsock -ladvapi32 -lrpcrt4 -luuid -loleaut32 -lole32 -lcomctl32 -lshell32 -lwinmm -lwinspool -lcomdlg32 -lgdi32 -luser32 -lkernel32 -lmingwthrd -lz -lws2_32 -pthread -lshlwapi -lnetapi32 -luserenv -lversion -luxtheme -ldwmapi -limm32'
ENV LDFLAGS '-L/usr/local/lib -L/usr/local/BerkeleyDB.4.8/lib -L/tmp/boost_1_58_0/stage/lib -L/tmp/miniupnpc-1.9.20160209 -L/tmp/zlib-1.2.11 -lssl -lcrypto -lz -static -levent'
ENV CFLAGS '-std=c++11 -I/usr/local/Qt-5.10.1/include/ -I/usr/local/BerkeleyDB.4.8/include -I/tmp/boost_1_58_0/boost -I/tmp/boost_1_58_0 -I/tmp -I/usr/local/include -I/tmp/zlib-1.2.11'
ENV CXXFLAGS $CFLAGS
ENV CPPFLAGS $CFLAGS
ENV BOOST_ROOT /tmp/boost_1_58_0

# TODO: change fork/branch
# RUN git clone https://github.com/vergecurrency/VERGE.git -b test-work ; cd verge ; ./autogen.sh --host=i686-w64-mingw32.static- 

RUN cd /tmp/verge && ./configure --with-gui-qt5 --host=${CROSS} --with-qt-incdir=/usr/local/Qt-5.10.1/include --with-qt-libdir=/usr/local/Qt-5.10.1/lib --with-qt-plugindir=/usr/local/Qt-5.10.1/plugins --with-qt-bindir=/usr/local/Qt-5.10.1/bin --without-tests --disable-tests  --without-bench --disable-bench --without-miniupnpc --disable-asm --includedir=/usr/local/include --includedir=/tmp/zlib-1.2.11 --includedir=/usr/local/Qt-5.10.1/include --includedir=/usr/local/BerkeleyDB.4.8/include --with-boost=/tmp/boost_1_58_0/boost --with-boost-libdir=/tmp/boost_1_58_0/stage/lib && make
