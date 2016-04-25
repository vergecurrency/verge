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

RUN /tmp/requirements.sh

#RUN git clone https://github.com/vergecurrency/verge /coin/git

