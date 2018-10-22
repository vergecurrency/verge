echo 'REMOVE OLD BREW COMPONENTS'
#brew install boost pkg-config
brew uninstall qt5

echo 'ADD REQUIRED BREW COMPONENTS'
# Make sure we have the C libs we need
brew install boost@1.60 pkg-config automake

echo 'ADD ADDITIONAL QT BREW COMPONENT'
# Install some deps
brew install qt@5.5 

echo 'ADD ADDITIONAL BREW COMPONENTS'
# Install even more deps
brew install protobuf miniupnpc openssl qrencode berkeley-db4

brew install moc

echo 'LINK BREW COMPONENTS'
# Make sure our stuff is linked in our path
brew link automake autoconf
