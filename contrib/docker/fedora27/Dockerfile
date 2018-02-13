FROM fedora:27

MAINTAINER Blpoing <blpoing@users.noreply.github.com>

RUN dnf upgrade -y
RUN dnf install -y autoconf automake gcc-c++ libdb4-cxx libdb4-cxx-devel boost-devel openssl-devel git bzip2 make file sudo tar patch findutils libevent-devel libseccomp-devel libcap-devel

RUN dnf install -y protobuf-lite-devel

RUN dnf install -y qt5-qtbase-devel qt5-qttools-devel qt5-qtwebkit-devel qt5-qtwebsockets qrencode-devel

RUN git clone https://github.com/vergecurrency/verge /coin/git

WORKDIR /coin/git

RUN ./autogen.sh && ./configure --with-incompatible-bdb --with-gui=qt5 && make && mv src/VERGEd /coin/VERGEd

WORKDIR /coin
VOLUME ["/coin/home"]

ENV HOME /coin/home

CMD ["/coin/VERGEd"]

# P2P, RPC
EXPOSE 21102 20102
