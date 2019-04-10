FROM ubuntu:18.04

MAINTAINER Ildar Gilmanov <gil.ildar@gmail.com>

ENV TARGET_PLATFORM win64

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

ENV DEBIAN_FRONTEND noninteractive

# We have different toolchains for Windows x32 and x64
ENV CROSS x86_64-w64-mingw32
ENV CC x86_64-w64-mingw32-gcc
ENV CXX x86_64-w64-mingw32-g++
ENV LD x86_64-w64-mingw32-ld
ENV AR x86_64-w64-mingw32-ar
ENV RANLIB x86_64-w64-mingw32-ranlib
ENV PKG_CONFIG x86_64-w64-mingw32-pkg-config

WORKDIR /tmp

RUN /tmp/requirements.sh

WORKDIR /verge

# To build the executable file you should clone the source codes and run /tmp/build.sh file

