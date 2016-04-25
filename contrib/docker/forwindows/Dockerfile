FROM ubuntu:14.04

MAINTAINER Mike Kinney <mike.kinney@gmail.com>

ADD ./requirements.sh /tmp/
ADD ./req_boost.sh /building/windows/req_boost.sh
ADD ./req_dbd.sh /building/windows/req_dbd.sh
ADD ./req_miniupnpc.sh /building/windows/req_miniupnpc.sh
ADD ./req_openssl.sh /building/windows/req_openssl.sh
ADD ./req_protobuf.sh /building/windows/req_protobuf.sh
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

# export CROSS=x86_64-w64-mingw32.static- CC=${CROSS}gcc CXX=${CROSS}g++ LD=${CROSS}ld AR=${CROSS}ar RANLIB=${CROSS}gcc-ranlib PKG_CONFIG=${CROSS}pkg-config PATH=/usr/lib/mxe/usr/bin/:$PATH

#./building/windows/req_dbd.sh
#./building/windows/req_miniupnpc.sh
#./building/windows/req_protobuf.sh
#./building/windows/req_boost.sh
#./building/windows/req_qrencode.sh
#./building/windows/req_qt.sh
