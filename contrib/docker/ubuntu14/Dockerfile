# Dockerfile for Verge
# http://vergecurrency.com/
# https://bitcointalk.org/index.php?topic=1365894
# https://github.com/vergecurrency/verge

# https://github.com/vergecurrency/Docker-Verge-Daemon.git

FROM ubuntu:14.04

MAINTAINER Jeremiah Buddenhagen <bitspill@bitspill.net>

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get install --yes -qq software-properties-common > /dev/null && \
    add-apt-repository --yes ppa:bitcoin/bitcoin && \
    apt-get update --yes > /dev/null && \
    apt-get upgrade --yes > /dev/null && \
    apt-get install --yes -qq \
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
       libdb4.8++-dev \
       libdb4.8-dev \
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
       qttools5-dev-tools \
      > /dev/null

# TODO: go back... RUN git clone https://github.com/vergecurrency/verge /coin/git
# This is temp during testing.
RUN git clone https://github.com/mkinney/verge -b test-work /coin/git

WORKDIR /coin/git

RUN ./autogen.sh && ./configure --with-gui=qt5 && make && mv src/VERGEd /coin/VERGEd

WORKDIR /coin
VOLUME ["/coin/home"]

ENV HOME /coin/home

CMD ["/coin/VERGEd"]

# P2P, RPC
EXPOSE 21102 20102
