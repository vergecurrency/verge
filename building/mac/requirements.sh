#brew install boost pkg-config
brew uninstall qt5
brew install autoconf automake libtool boost miniupnpc openssl pkg-config protobuf qrencode berkeley-db4
cd ..
db-4.8.30/dist/configure --prefix=/usr/local/Cellar/berkeley-db4/4.8.30 --mandir=/usr/local/Cellar/berkeley-db4/4.8.30/share/man --enable-cxx
make
make install
exit
brew link berkeley-db4 --force
# Might need these later: libevent librsvg
curl -O https://raw.githubusercontent.com/Homebrew/homebrew-core/fdfc724dd532345f5c6cdf47dc43e99654e6a5fd/Formula/qt5.rb
brew install ./qt5.rb
brew link --force qt5
