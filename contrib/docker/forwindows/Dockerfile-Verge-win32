FROM ubuntu:18.04

MAINTAINER Ildar Gilmanov <gil.ildar@gmail.com>

ENV TARGET_PLATFORM win32

ADD ./requirements.sh /tmp/
ADD ./req_boost.sh /tmp/
ADD ./req_dbd.sh /tmp/
ADD ./req_miniupnpc.sh /tmp/
ADD ./req_openssl.sh /tmp/
ADD ./req_protobuf.sh /tmp/
ADD ./req_zlib.sh /tmp/
ADD ./req_qrencode.sh /tmp/
ADD ./req_libevent.sh /tmp/
ADD ./req_qt.sh /tmp/
ADD ./build.sh /tmp/

# We should have this patch for Windows x32 build only
# Later we can try to build qt without this patch
ADD ./disable-qt_random_cpu_new.patch /tmp/

ENV DEBIAN_FRONTEND noninteractive

# We have different toolchains for Windows x32 and x64
ENV CROSS i686-w64-mingw32
ENV CC i686-w64-mingw32-gcc
ENV CXX i686-w64-mingw32-g++
ENV LD i686-w64-mingw32-ld
ENV AR i686-w64-mingw32-ar
ENV RANLIB i686-w64-mingw32-ranlib
ENV PKG_CONFIG i686-w64-mingw32-pkg-config

WORKDIR /tmp

RUN /tmp/requirements.sh

WORKDIR /verge

# To build the executable file you should clone the source codes and run /tmp/build.sh file

