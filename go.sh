#!/bin/bash
#// full deployement :   wget -O - https://raw.githubusercontent.com/vergecurrency/VERGE/master/go.sh | bash
var=`date +"%FORMAT_STRING"`
now=`date +"%m_%d_%Y"`
now=`date +"%Y-%m-%d-%s"`

if [ -e ~/.VERGE/VERGE.conf ]
then
    cp -a ~/.VERGE/VERGE.conf ~/VERGE.conf
    cp -a ~/.VERGE/VERGE.conf ~/VERGE.conf${now}
fi
if [ -e ~/.VERGE/wallet.dat ]
then
    cp -a ~/.VERGE/wallet.dat ~/wallet.dat
    cp -a ~/.VERGE/wallet.dat ~/Vwallet.dat${now}
fi

sudo rm -Rf ~/VERGE
# generating entropy make it harder to guess the randomness!.
echo "Initializing random number generator..."
random_seed=/var/run/random-seed
# Carry a random seed from start-up to start-up
# Load and then save the whole entropy pool
if [ -f $random_seed ]
then
    sudo cat $random_seed >/dev/urandom
else
    sudo touch $random_seed
fi
sudo chmod 600 $random_seed
poolfile=/proc/sys/kernel/random/poolsize
[ -r $poolfile ] && bytes=`sudo cat $poolfile` || bytes=512
sudo dd if=/dev/urandom of=$random_seed count=1 bs=$bytes

#Also, add the following lines in an appropriate script which is run during the$

# Carry a random seed from shut-down to start-up
# Save the whole entropy pool
echo "Saving random seed..."
random_seed=/var/run/random-seed
sudo touch $random_seed
sudo chmod 600 $random_seed
poolfile=/proc/sys/kernel/random/poolsize
[ -r $poolfile ] && bytes=`sudo cat $poolfile` || bytes=512
sudo dd if=/dev/urandom of=$random_seed count=1 bs=$bytes

# Create a swap file

cd ~
if [ -e /swapfile1 ]
then
echo "Swapfile already present"
else
sudo dd if=/dev/zero of=/swapfile1 bs=1024 count=1024288
sudo mkswap /swapfile1
sudo chown root:root /swapfile1
sudo chmod 0600 /swapfile1
sudo swapon /swapfile1
fi

# Install dependency

sudo apt-get -y install software-properties-common

sudo add-apt-repository -y ppa:bitcoin/bitcoin

sudo apt-get update

sudo apt-get -y install libcanberra-gtk-module

# Dont need to check if bd is already installed, will override or pass by
#results=$(find /usr/ -name libdb_cxx.so)
#if [ -z $results ]; then
sudo apt-get -y install libdb4.8-dev libdb4.8++-dev
#else
#grep DB_VERSION_STRING $(find /usr/ -name db.h)
#echo "BerkeleyDb will not be installed its already there...."
#fi

sudo apt-get -y install libtool autotools-dev automake pkg-config libssl-dev libevent-dev

sudo apt-get -y install bsdmainutils git libboost-all-dev libminiupnpc-dev libqt5gui5

sudo apt-get -y install libqt5core5a libqt5dbus5 libevent-dev qttools5-dev

sudo apt-get -y install qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev

sudo apt-get -y install zlib1g-dev libseccomp-dev libcap-dev libncap-dev

sudo apt-get -y install libunivalue-dev libzmq3-dev

sudo apt-get -y install g++ build-essential

# Keep current version of libboost if already present
results=$(find /usr/ -name libboost_chrono.so)
if [ -z $results ]
then
sudo apt-get -y install libboost-all-dev
else
red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`
echo "${green}Libboost will not be installed its already there....${reset}"
grep --include=*.hpp -r '/usr/' -e "define BOOST_LIB_VERSION"
fi

sudo apt-get -y install --no-install-recommends gnome-panel

sudo apt-get -y install lynx

sudo apt-get -y install unzip

sudo apt-get -y install sed

cd ~

#// Compile Berkeley if 4.8 is not there
if [ -e /usr/lib/libdb_cxx-4.8.so ]
then
red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`
echo "${green}BerkeleyDb already present...$(grep --include *.h -r '/usr/' -e 'DB_VERSION_STRING')${reset}" 
else
wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz 
tar -xzvf db-4.8.30.NC.tar.gz
result5=$(cat /etc/issue | grep -Po '20.04')
if [ $result5 = "20.04" ]
then
sed -i 's/__atomic_compare_exchange/__db_atomic_compare_exchange/g' ~/db-4.8.30.NC/dbinc/atomic.h
fi
result2=$(cat /etc/issue | grep -Po '19.04')
if [ $result2 = "19.04" ]
then
sed -i 's/__atomic_compare_exchange/__db_atomic_compare_exchange/g' ~/db-4.8.30.NC/dbinc/atomic.h
fi
result3=$(cat /etc/issue | grep -Po '4.6')
if [ $result3 = "4.6" ]
then
sed -i 's/__atomic_compare_exchange/__db_atomic_compare_exchange/g' ~/db-4.8.30.NC/dbinc/atomic.h
fi
result4=$(cat /etc/issue | grep -Po 'Ermine')
if [ $result4 = "Ermine" ]
then
sed -i 's/__atomic_compare_exchange/__db_atomic_compare_exchange/g' ~/db-4.8.30.NC/dbinc/atomic.h
fi

rm db-4.8.30.NC.tar.gz
cd db-4.8.30.NC/build_unix 
../dist/configure --enable-cxx
make 
sudo make install 
sudo ln -s /usr/local/BerkeleyDB.4.8/lib/libdb-4.8.so /usr/lib/libdb-4.8.so
sudo ln -s /usr/local/BerkeleyDB.4.8/lib/libdb_cxx-4.8.so /usr/lib/libdb_cxx-4.8.so
cd ~
sudo rm -Rf db-4.8.30.NC
#sudo ldconfig
fi

#// Check if libboost is present

results=$(find /usr/ -name libboost_chrono.so)
if [ -z $results ]
then
sudo rm download
     wget https://sourceforge.net/projects/boost/files/boost/1.67.0/boost_1_67_0.zip/download 
     unzip -o download
     cd boost_1_67_0
	sh bootstrap.sh
	sudo ./b2 install
	cd ~
	sudo rm download 
	sudo rm -Rf boost_1_67_0
	#sudo ln -s $(dirname "$(find /usr/ -name libboost_chrono.so)")/lib*.so /usr/lib
	sudo ldconfig
        #sudo rm /usr/lib/libboost_chrono.so
else
     echo "Libboost found..." 
     grep --include=*.hpp -r '/usr/' -e "define BOOST_LIB_VERSION"
fi

echo Libssl version: $(/usr/bin/openssl version)
#// Clone files from repo, Permissions and make

git clone --recurse-submodules https://github.com/vergecurrency/VERGE
cd ~
cd VERGE
./autogen.sh
chmod 777 ~/VERGE/share/genbuild.sh
chmod 777 ~/VERGE/src/leveldb/build_detect_platform

grep --include=*.hpp -r '/usr/' -e "define BOOST_LIB_VERSION"

sudo rm wrd01.txt
sudo rm wrd00.txt
sudo rm words
find /usr/ -name libboost_chrono.so > words
split -dl 1 --additional-suffix=.txt words wrd

if [ -e wrd01.txt ]
then
echo 0. $(cat wrd00.txt)
echo 1. $(cat wrd01.txt)
echo 2. $(cat wrd02.txt)
echo 3. $(cat wrd03.txt)
echo -n "Choose libboost library to use(0-3)?"
read answer
else
echo "There is only 1 libboost library present. We choose for you 0"
answer=0
fi

echo "You have choosen $answer"

if [ $(dirname "$(cat wrd00.txt)") = "/usr/lib/arm-linux-gnueabihf" ]
then
red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`
echo "${green}ARM cpu detected --disable-sse2${reset}"
txt=$(echo "--disable-sse2")
sed -i 's/#include <emmintrin.h>//g' ~/VERGE/src/crypto/pow/scrypt-sse2.cpp
else
txt=$(echo "")
fi

if [ -d /usr/local/BerkeleyDB.4.8/include ]
then
cd VERGE
./configure CXXFLAGS="--param ggc-min-expand=1 --param ggc-min-heapsize=32768" CPPFLAGS="-I/usr/local/BerkeleyDB.4.8/include -O2" LDFLAGS="-L/usr/local/BerkeleyDB.4.8/lib" --with-gui=qt5 --with-boost-libdir=$(dirname "$(cat wrd0$answer.txt)") --disable-bench --disable-tests --disable-gui-tests --without-miniupnpc $txt
echo "Using Berkeley Generic..."
else
cd VERGE
./configure CXXFLAGS="--param ggc-min-expand=1 --param ggc-min-heapsize=32768" CPPFLAGS="-O2" --with-gui=qt5 --with-boost-libdir=$(dirname "$(cat wrd0$answer.txt)") --disable-bench --disable-tests --disable-gui-tests --without-miniupnpc $txt

echo "Using default system Berkeley..."
fi

#make -j$(nproc) USE_UPNP=-
make USE_UPNP=-

if [ -e ~/VERGE/src/qt/verge-qt ]
then
sudo strip ~/VERGE/src/verged
sudo strip ~/VERGE/src/qt/verge-qt
sudo make install
else
echo "Compile fail not VERGE-qt present"
fi

cd ~

#// Create the config file with random user and password

mkdir -p ~/.VERGE
if [ -e ~/.VERGE/VERGE.conf ]
then
    cp -a ~/.VERGE/VERGE.conf ~/.VERGE/VERGE.bak
fi
rm ~/.VERGE/VERGE.conf
echo "rpcuser="$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 26) >> ~/.VERGE/VERGE.conf
echo "rpcpassword="$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 26) >> ~/.VERGE/VERGE.conf
echo "rpcport=20102" >> ~/.VERGE/VERGE.conf
echo "port=21102" >> ~/.VERGE/VERGE.conf
echo "daemon=1" >> ~/.VERGE/VERGE.conf
echo "listen=1" >> ~/.VERGE/VERGE.conf
echo "server=1" >> ~/.VERGE/VERGE.conf
echo "deprecatedrpc=accounts" >> ~/.VERGE/VERGE.conf

echo "usehd=1" >> ~/.VERGE/VERGE.conf


#// Extract http link, download blockchain and install it.

sudo chmod 777 /etc/lynx/lynx.cfg
sudo echo "FORCE_SSL_PROMPT:YES" >> /etc/lynx/lynx.cfg

# Create Icon on Desktop and in menu
mkdir -p ~/Desktop/
sudo cp ~/VERGE/src/qt/res/icons/verge.png /usr/share/icons/
echo "#!/usr/bin/env xdg-open" >> ~/Desktop/VERGE.desktop
echo "[Desktop Entry]" >> ~/Desktop/VERGE.desktop
echo "Version=1.0" >> ~/Desktop/VERGE.desktop
echo "Type=Application" >> ~/Desktop/VERGE.desktop
echo "Terminal=false" >> ~/Desktop/VERGE.desktop
echo "Icon[en]=/usr/share/icons/verge.png" >> ~/Desktop/VERGE.desktop
echo "Name[en]=VERGE" >> ~/Desktop/VERGE.desktop
echo "Exec=verge-qt" >> ~/Desktop/VERGE.desktop
echo "Name=VERGE" >> ~/Desktop/VERGE.desktop
echo "Icon=/usr/share/icons/verge.png" >> ~/Desktop/VERGE.desktop
echo "Categories=Network;Internet;" >> ~/Desktop/VERGE.desktop
sudo chmod +x ~/Desktop/VERGE.desktop
sudo cp ~/Desktop/VERGE.desktop /usr/share/applications/VERGE.desktop
sudo chmod +x /usr/share/applications/VERGE.desktop

if [ -e ~/VERGE.conf ]
then
    cp -a ~/VERGE.conf ~/.VERGE/VERGE.conf
    rm ~/VERGE.conf
fi
if [ -e ~/wallet.dat ]
then
    cp -a ~/wallet.dat ~/.VERGE/wallet.dat
    rm ~/wallet.dat
fi

# Erase all VERGE compilation directory , cleaning

cd ~
#sudo rm -Rf ~/VERGE

# Blockchain

echo -n "Success....Blockchain is now downloading press Ctrl-C to cancel but it will take longer to sync from 0. And you will have to start verge manual"
sudo rm QT-Wallet*.zip
echo "wget --no-check-certificate " $(lynx --dump --listonly https://verge-blockchain.com/download/ | grep -o "https://verge-blockchain*.*zip" | head -1 ) > link.sh
sh link.sh
sudo rm -Rf ~/.VERGE/blocks
sudo rm -Rf ~/.VERGE/chainstate
sudo rm -Rf ~/.VERGE/tor
sudo rm ~/.VERGE/peers.dat
unzip -o QT-Wallet*.zip -d ~/.VERGE
sudo rm QT-Wallet*.zip
#// Start Verge
verge-qt
