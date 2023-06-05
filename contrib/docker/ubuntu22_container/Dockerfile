FROM ubuntu:22.04

RUN apt-get update

RUN apt-get install -y build-essential bison byacc
RUN apt-get install -y git
RUN apt-get install -y software-properties-common python-software-properties 
RUN cd ~
RUN wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
RUN tar -xzvf db-4.8.30.NC.tar.gz
RUN cd db-4.8.30.NC/build_unix/
RUN sed -i 's/__atomic_compare_exchange/__atomic_compare_exchange_db/g' ~/db-4.8.30.NC/dbinc/atomic.h
RUN ../dist/configure --enable-cxx && make && make install
RUN sudo ln -s /usr/local/BerkeleyDB.4.8/lib/libdb-4.8.so /usr/lib/libdb-4.8.so
RUN sudo ln -s /usr/local/BerkeleyDB.4.8/lib/libdb_cxx-4.8.so /usr/lib/libdb_cxx-4.8.so

RUN add-apt-repository ppa:bitcoin/bitcoin
RUN apt-get update

RUN apt-get install -y libtool autotools-dev automake pkg-config libssl-dev libevent-dev \
    bsdmainutils git libboost-all-dev libminiupnpc-dev libqt5gui5 libqt5core5a libqt5webkit5-dev libqt5dbus5 qtbase5-dev qttools5-dev qttools5-dev-tools \
    libprotobuf-dev protobuf-compiler libqrencode-dev zlib1g-dev libseccomp-dev libcap-dev libncap-dev

#verge
RUN mkdir /root/VERGE
RUN git clone https://github.com/vergecurrency/VERGE /root/VERGE
RUN cd /root/VERGE/ ; /root/VERGE/autogen.sh
RUN cd /root/VERGE/ ; /root/VERGE/configure
RUN cd /root/VERGE/ ; make

RUN chmod 0777 /root/VERGE/src/*

ADD ./conf/VERGE.conf /root/.VERGE/VERGE.conf

WORKDIR /root/VERGE

EXPOSE 21102 20102

#CMD printenv | grep -v "yyyyyxxxx" >> /etc/environment && tail -f /dev/null

CMD /root/VERGE/src/VERGEd -printtoconsole
