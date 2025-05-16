# Dockerfile for Verge Daemon
# https://vergecurrency.com/
# https://github.com/vergecurrency/verge

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Set base working directory
WORKDIR /coin

# Install dependencies
RUN apt-get update && \
    apt-get install -y software-properties-common && \
    apt-get update && \
    apt-get install -y \
    build-essential \
    git \
    libboost-all-dev \
    libtool \
    autotools-dev \
    automake \
    pkg-config \
    libssl-dev \
    libevent-dev \
    bsdmainutils \
    libminiupnpc-dev \
    libqt5gui5 \
    libqt5core5a \
    libqt5dbus5 \
    qttools5-dev \
    qttools5-dev-tools \
    libprotobuf-dev \
    protobuf-compiler \
    zlib1g-dev \
    libseccomp-dev \
    libcap-dev \
    libncap-dev \
    libqrencode-dev \
    curl

# Clone Verge source
RUN git clone https://github.com/vergecurrency/verge.git /coin/verge

# Build db4 from source
WORKDIR /coin/verge/contrib
RUN ./install_db4.sh ..

# list files
WORKDIR /coin/verge/contrib
RUN ls

# Configure and build Verge
WORKDIR /coin/verge
RUN ./autogen.sh && \
    ./configure LDFLAGS="-L/coin/verge/db4/lib/" CPPFLAGS="-I/coin/verge/db4/include/" --disable-bench --disable-tests --disable-dependency-tracking --disable-werror --with-gui=no && \
    make -j$(nproc)

# Move final binary to standard location
RUN mv /coin/verge/src/verged /coin/vergedaemon && \
    rm -rf /coin/verge

# Define runtime environment
WORKDIR /coin
VOLUME ["/coin/home"]
ENV HOME /coin/home

# Default command
ENTRYPOINT ["/coin/vergedaemon"]

# Expose Verge P2P and RPC ports
EXPOSE 21102 20102
