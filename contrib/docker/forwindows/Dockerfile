FROM ubuntu:14.04

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
ENV LDFLAGS '-lssl -lcrypto -static -L/tmp/db-4.8.30.NC/build_windows -L/tmp/boost_1_55_0/stage/lib -L/tmp/miniupnpc-1.9.20160209 -L/tmp/protobuf-2.5.0/src/.libs/ -L/tmp/zlib-1.2.8'
ENV CFLAGS '-I/tmp/db-4.8.30.NC/build_windows -I/tmp/boost_1_55_0/boost -I/tmp -I/tmp/protobuf-2.5.0 -I/tmp/zlib-1.2.8'
ENV CXXFLAGS $CFLAGS
ENV CPPFLAGS $CFLAGS
ENV BOOST_ROOT /tmp/boost_1_55_0
# TODO: change fork/branch
RUN git clone https://github.com/mkinney/verge -b test-work ; cd verge ; ./autogen.sh --host=i686-w64-mingw32.static- 

RUN cd /tmp/verge && ./configure --host=i686-w64-mingw32.static && make

