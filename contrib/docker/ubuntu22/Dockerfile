# Dockerfile for Verge
# http://vergecurrency.com/
# https://bitcointalk.org/index.php?topic=1365894
# https://github.com/vergecurrency/verge

# https://github.com/vergecurrency/Docker-Verge-Daemon.git
#  Jeremiah Buddenhagen <bitspill@bitspill.net>

FROM ubuntu:22.04

MAINTAINER Mike Kinney <mike.kinney@gmail.com> / Sunerok

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update --yes > /dev/null && \
    apt-get upgrade --yes > /dev/null && \
    apt-get install --yes -qq software-properties-common > /dev/null && \
    cd ~ \
	wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz' \
	tar -xzvf db-4.8.30.NC.tar.gz \
	cd db-4.8.30.NC/build_unix/ \
	sed -i 's/__atomic_compare_exchange/__atomic_compare_exchange_db/g' ~/db-4.8.30.NC/dbinc/atomic.h \
	../dist/configure --enable-cxx && make && make install \
	sudo ln -s /usr/local/BerkeleyDB.4.8/lib/libdb-4.8.so /usr/lib/libdb-4.8.so \
    sudo ln -s /usr/local/BerkeleyDB.4.8/lib/libdb_cxx-4.8.so /usr/lib/libdb_cxx-4.8.so \
	cd ~ \
    apt-get install --yes -qq \
       autoconf \
       automake \
       autotools-dev \
	   bison \
	   byacc \
       bsdmainutils \
       build-essential \
       git \
       libboost-all-dev \
       libboost-filesystem-dev \
       libboost-program-options-dev \
       libboost-system-dev \
       libboost-test-dev \
       libboost-thread-dev \
       libcap-dev \
       libncap-dev \
       libdb4.8++-dev \
       libdb4.8-dev \
       libevent-dev \
       libminiupnpc-dev \
       libprotobuf-dev \
       libqrencode-dev \
       libqt5core5a \
       libqt5dbus5 \
       libqt5gui5 \
       libseccomp-dev \
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
       zlib1g-dev \
      > /dev/null

RUN git clone https://github.com/vergecurrency/VERGE.git /coin/git

WORKDIR /coin/git

RUN ./autogen.sh && ./configure --with-gui=qt5 && make && mv src/verged /coin/verged

WORKDIR /coin
VOLUME ["/coin/home"]

ENV HOME /coin/home

CMD ["/coin/verged"]

# P2P, RPC
EXPOSE 21102 20102
