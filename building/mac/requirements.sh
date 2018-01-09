# Install our depdencies some deps
brew install pkg-config \
  automake \
  boost@1.60 \
  qt@5.5 \
  protobuf \
  miniupnpc \
  openssl \
  qrencode \
  berkeley-db4 \
  zlib \
  libevent

# Make sure our stuff is linked in our path
brew link automake autoconf
