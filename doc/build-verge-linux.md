### Building Linux Wallet on Ubuntu/Debian

Here is a quick workaround:

```shell
sudo apt-get -y install git && test -d $HOME/VERGE && rm -rf $HOME/VERGE && \
         git clone --recurse-submodules https://github.com/vergecurrency/VERGE $HOME/VERGE && \
         cd $HOME/VERGE && ./go.sh
```

The _slightly_ longer version:

#### Ubuntu:

1. Installing dependencies:

```shell
sudo add-apt-repository ppa:bitcoin/bitcoin && \
         sudo apt-get update && \
         sudo apt-get -y install git libdb4.8-dev libdb4.8++-dev build-essential libtool \
         autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils \
         libboost-all-dev libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 \
         qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev \
         libseccomp-dev libcap-dev
```

2. Cloning git repository:

```shell
test -d $HOME/VERGE && rm -rf $HOME/VERGE && \
         git clone --recurse-submodules https://github.com/vergecurrency/VERGE $HOME/VERGE
```

3. Installing:

```shell
test "$(cat /proc/swaps | wc -l)" = "1" && \
         sudo dd if=/dev/zero of=/temp_swap bs=1M count=1024 && sudo chmod 600 /temp_swap && \
         sudo mkswap /temp_swap && sudo swapon /temp_swap; \
         cd $HOME/VERGE && ./autogen.sh && \
         ./configure --with-gui=qt5 && \
         make && sudo make install && \
         test -e /temp_swap && sudo swapoff /temp_swap && sudo rm -f /temp_swap
```

4. Menu icon:

```shell
sudo cp -f $HOME/VERGE/src/qt/res/icons/verge.png /usr/share/icons/ && \
         printf "[Desktop Entry]\nType=Application\nName=VERGE\nGenericName=VERGE Qt Wallet\nIcon=/usr/share/icons/verge.png\nCategories=Network;Internet;\nExec=VERGE-qt\nTerminal=false" > $HOME/VERGE/VERGE.desktop && \
         sudo cp -f $HOME/VERGE/VERGE.desktop /usr/share/applications/
```

#### Debian:

1. Installing dependencies:

```shell
sudo apt-get update && \
         sudo apt-get -y install git software-properties-common libcanberra-gtk-module \
         build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev \
         bsdmainutils libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 \
         qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev \
         libseccomp-dev libcap-dev
```

As it is stated <a href="https://launchpad.net/~bitcoin/+archive/ubuntu/bitcoin">here</a>:

         No longer supports precise, due to its ancient gcc and Boost versions.

But you can install BerkeleyDB-4.8 from Trusty repository, e.g.:

```shell
sudo apt-get -y install dirmngr && \
         sudo add-apt-repository "deb http://ppa.launchpad.net/bitcoin/bitcoin/ubuntu trusty main" && \
         sudo apt-get update >> /dev/null 2> /tmp/temp_apt_key.txt && \
         TEMP_APT_KEY=$(cat /tmp/temp_apt_key.txt | cut -d":" -f6 | cut -d" " -f3) && \
         sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys $TEMP_APT_KEY && \
         sudo rm -f /tmp/temp_apt_key.txt && \
         sudo apt-get -y install libdb4.8-dev libdb4.8++-dev && \
         BERKELEY_ARG="/usr"      
```

Or compile BerkeleyDB-4.8 manually, e.g:

```shell
wget -O $HOME/db-4.8.30.NC.tar.gz https://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz && \
         tar -C $HOME -xzvf $HOME/db-4.8.30.NC.tar.gz && \
         sudo sed -i 's/__atomic_compare_exchange(/__atomic_compare_exchange_db(/g' $HOME/db-4.8.30.NC/dbinc/atomic.h && \
         cd $HOME/db-4.8.30.NC/build_unix && \
         ../dist/configure --enable-cxx && \
         make && sudo make install && \
         sudo ln -s /usr/local/BerkeleyDB.4.8/lib/libdb-4.8.so /usr/lib/libdb-4.8.so && \
         sudo ln -s /usr/local/BerkeleyDB.4.8/lib/libdb_cxx-4.8.so /usr/lib/libdb_cxx-4.8.so && \
         cd $HOME && \
         rm -rf db-4.8.30.NC && \
         BERKELEY_ARG="/usr/local/BerkeleyDB.4.8"
```

Also, you need boost(>=1.63), e.g.:

```shell
wget -O $HOME/boost_1_63_0.zip https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.zip && \
         unzip $HOME/boost_1_63_0.zip -d $HOME/boost_1_63_0 && cd $HOME/boost_1_63_0 && \
         ./bootstrap.sh && \
         sudo ./b2 install && \
         cd .. && \
         rm -rf boost_1_63_0
```

2. Cloning git repository:

```shell
test -d $HOME/VERGE && rm -rf $HOME/VERGE; \
         git clone --recurse-submodules https://github.com/vergecurrency/VERGE $HOME/VERGE
```

3. Installing:

```shell
test "$(cat /proc/swaps | wc -l)" = "1" && \
         sudo dd if=/dev/zero of=/temp_swap bs=1M count=1024 && sudo chmod 600 /temp_swap && \
         sudo mkswap /temp_swap && sudo swapon /temp_swap; \
         cd $HOME/VERGE && ./autogen.sh && \
         ./configure CPPFLAGS="-I$BERKELEY_ARG/include -O2" LDFLAGS="-L$BERKELEY_ARG/lib" --with-gui=qt5 --with-boost-libdir="/usr/local/lib/" && \
         make && sudo make install && \
         test -e /temp_swap && sudo swapoff /temp_swap && sudo rm -f /temp_swap
```

4. Menu icon:

```shell
sudo cp -f $HOME/VERGE/src/qt/res/icons/verge.png /usr/share/icons/ && \
         printf "[Desktop Entry]\nType=Application\nName=VERGE\nGenericName=VERGE Qt Wallet\nIcon=/usr/share/icons/verge.png\nCategories=Network;Internet;\nExec=VERGE-qt\nTerminal=false" > $HOME/VERGE/VERGE.desktop && \
         sudo cp -f $HOME/VERGE/VERGE.desktop /usr/share/applications/
```

### Building Linux Wallet on Fedora 27

This should also work on some previous Fedoras if you replace dnf with yum.

1. Installing dependencies:

```shell
sudo apt-get update && \
         sudo dnf install git automake boost-devel qt5-devel qrencode-devel libdb4-cxx-devel \
         miniupnpc-devel protobuf-devel gcc gcc-c++ openssl-devel libcap-devel libseccomp-devel \
         libseccomp libevent-devel
```

2. Cloning git repository:

```shell
test -d $HOME/VERGE && rm -rf $HOME/VERGE; \
         git clone --recurse-submodules https://github.com/vergecurrency/VERGE $HOME/VERGE
```

3. Installing:

```shell
test "$(cat /proc/swaps | wc -l)" = "1" && \
         sudo dd if=/dev/zero of=/temp_swap bs=1M count=1024 && sudo chmod 600 /temp_swap && \
         sudo mkswap /temp_swap && sudo swapon /temp_swap; \
         cd $HOME/VERGE && ./autogen.sh && \
         ./configure --with-gui=qt5 && \
         make && sudo make install && \
         test -e /temp_swap && sudo swapoff /temp_swap && sudo rm -f /temp_swap
```

4. Menu icon:

```shell
sudo cp -f $HOME/VERGE/src/qt/res/icons/verge.png /usr/share/icons/ && \
         printf "[Desktop Entry]\nType=Application\nName=VERGE\nGenericName=VERGE Qt Wallet\nIcon=/usr/share/icons/verge.png\nCategories=Network;Internet;\nExec=VERGE-qt\nTerminal=false" > $HOME/VERGE/VERGE.desktop && \
         sudo cp -f $HOME/VERGE/VERGE.desktop /usr/share/applications/
```
