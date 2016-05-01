# Dockerfile for Verge

FROM debian:8.4

MAINTAINER Mike Kinney <mike.kinney@gmail.com>

ENV DEBIAN_FRONTEND noninteractive

ADD http://httpredir.debian.org/debian/project/trace/ftp-master.debian.org /etc/trace_ftp-master.debian.org
ADD http://security.debian.org/project/trace/security-master.debian.org /etc/trace_security-master.debian.org
RUN apt-get update && apt-get -y dist-upgrade

RUN apt-get -y install \
       autoconf \
       automake \
       autotools-dev \
       bsdmainutils \
       build-essential \
       git \
       libboost-all-dev \
       libboost-filesystem-dev \
       libboost-program-options-dev \
       libboost-system-dev \
       libboost-test-dev \
       libboost-thread-dev \
       libcanberra-gtk-module \
       libdb++-dev \
       libdb-dev \
       libevent-dev \
       libminiupnpc-dev \
       libprotobuf-dev \
       libqrencode-dev \
       libqt5core5a \
       libqt5dbus5 \
       libqt5gui5 \
       libqt5webkit5-dev  \
       libsqlite3-dev \
       libssl-dev \
       libtool \
       pkg-config \
       protobuf-compiler \
       qt5-default \
       qtbase5-dev \
       qtdeclarative5-dev \
       qttools5-dev \
       qttools5-dev-tools

# TODO: go back... RUN git clone https://github.com/vergecurrency/verge /coin/git
# This is temp during testing.
RUN git clone https://github.com/mkinney/verge -b test-work /coin/git

WORKDIR /coin/git

RUN ./autogen.sh && ./configure --with-gui=qt5 --with-incompatible-bdb && make && mv src/VERGEd /coin/VERGEd

WORKDIR /coin
VOLUME ["/coin/home"]

ENV HOME /coin/home

CMD ["/coin/VERGEd"]

# P2P, RPC
EXPOSE 21102 20102
