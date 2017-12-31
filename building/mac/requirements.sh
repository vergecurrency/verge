#brew install boost pkg-config 
brew uninstall qt5
brew install protobuf miniupnpc openssl qrencode berkeley-db4 
# Manually link bekeley-db4 due to known bug - https://github.com/bitcoin/bitcoin/issues/3550
brew link --force berkeley-db4
# Might need these later: libevent librsvg
curl -O https://raw.githubusercontent.com/Homebrew/homebrew-core/fdfc724dd532345f5c6cdf47dc43e99654e6a5fd/Formula/qt5.rb
brew install ./qt5.rb
brew link --force qt5

