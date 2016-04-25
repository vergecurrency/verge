
sudo apt-get --yes -qq install software-properties-common > /dev/null
sudo add-apt-repository --yes ppa:ubuntu-sdk-team/ppa > /dev/null
sudo add-apt-repository --yes ppa:bitcoin/bitcoin > /dev/null
sudo apt-get update -qq > /dev/null

sudo apt-get --yes -qq install qtbase5-dev qtdeclarative5-dev libqt5webkit5-dev libsqlite3-dev zip > /dev/null
sudo apt-get --yes -qq install qt5-default qttools5-dev-tools libdb4.8-dev libdb4.8++-dev > /dev/null
sudo apt-get --yes -qq install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils git libboost-system-dev libboost-filesystem-dev libqrencode-dev libboost-program-options-dev libboost-test-dev libboost-thread-dev libboost-all-dev libprotobuf-dev protobuf-compiler libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev > /dev/null



