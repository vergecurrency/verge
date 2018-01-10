### Building Linux Wallet on Ubuntu/Debian

Here is a quick short hand way:

```shell
sudo rm -Rf ~/VERGE  #(if you already have it)
sudo apt-get -y install git && cd ~ && git clone https://github.com/vergecurrency/VERGE && cd VERGE && sh go.sh
```

The _slightly_ longer version:

1. Install the dependencies. **Note**: If you are on debian, you will also need to `apt-get install libcanberra-gtk-module`.

    ```shell
    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get update
    sudo apt-get install \
        libdb4.8-dev libdb4.8++-dev build-essential \
        libtool autotools-dev automake pkg-config libssl-dev zlib1g-dev libz-dev libevent-dev \
        bsdmainutils git libboost-all-dev libseccomp-dev libcap-dev libminiupnpc-dev libqt5gui5 \
        libqt5core5a libqt5webkit5-dev libqt5dbus5 qttools5-dev \
        qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev
    ```

2. Clone the git repository and compile the daemon and gui wallet:

    ```shell
    git clone https://github.com/vergecurrency/verge && cd verge && ./autogen.sh && ./configure && make
    ```

> **Note**: If you get a "memory exhausted" error, make a swap file. (https://www.digitalocean.com/community/tutorials/how-to-add-swap-space-on-ubuntu-16-04)


### Building Linux Wallet on Fedora 27

This should also work on some previous Fedoras if you replace `dnf` with `yum`.

1. Install dependencies
```
sudo dnf install git automake boost-devel qt5-devel qrencode-devel libdb4-cxx-devel miniupnpc-devel protobuf-devel gcc gcc-c++ openssl-devel
```
2. Clone source and build
```
git clone https://github.com/vergecurrency/VERGE.git
cd VERGE
./autogen.sh
./configure --with-gui=qt5
make -j4 # number of CPU cores to use for compiling
sudo make install
```
3. Run `VERGE-qt`
