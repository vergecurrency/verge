#!/bin/bash
#// full deployement : run sh go.sh

# generating entropy make it harder to guess the randomness!.
echo "Initializing random number generator..."
random_seed=/var/run/random-seed
# Carry a random seed from start-up to start-up
# Load and then save the whole entropy pool
if [ -f $random_seed ]; then
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
if [ -e /swapfile1 ]; then
echo "Swapfile already present"
else
sudo dd if=/dev/zero of=/swapfile1 bs=1024 count=524288
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

sudo apt-get -y install git build-essential libtool autotools-dev autoconf automake pkg-config libssl-dev libevent-dev bsdmainutils git libprotobuf-dev protobuf-compiler libqrencode-dev

sudo apt-get -y install libqt5gui5 libqt5core5a libqt5webkit5-dev libqt5dbus5 qttools5-dev qttools5-dev-tools

sudo apt-get -y install libminiupnpc-dev

# Keep current version of libboost if already present
results=$(find /usr/ -name libboost_chrono.so)
if [ -z $results ]; then
sudo apt-get -y install libboost-all-dev
else
red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`
echo "${red}Libboost will not be installed its already there....${reset}"
grep --include=*.hpp -r '/usr/' -e "define BOOST_LIB_VERSION"
fi

sudo apt-get -y install --no-install-recommends gnome-panel

sudo apt-get -y install lynx

sudo apt-get -y install unzip

cd ~

#// Compile Berkeley if 4.8 is not there
if [ -e /usr/lib/libdb_cxx-4.8.so ]
then
echo "BerkeleyDb already present...$(grep --include *.h -r '/usr/' -e 'DB_VERSION_STRING')" 
else
wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz 
tar -xzvf db-4.8.30.NC.tar.gz 
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
if [ -z $results ]; then
sudo rm download
     wget https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.zip/download 
     unzip -o download
     cd boost_1_63_0
	sh bootstrap.sh
	sudo ./b2 install
	cd ~
	sudo rm download 
	sudo rm -Rf boost_1_63_0
	#sudo ln -s $(dirname "$(find /usr/ -name libboost_chrono.so)")/lib*.so /usr/lib
	sudo ldconfig
        #sudo rm /usr/lib/libboost_chrono.so
else
     echo "Libboost found..." 
     grep --include=*.hpp -r '/usr/' -e "define BOOST_LIB_VERSION"
fi

#// Clone files from repo, Permissions and make

git clone --recurse-submodules https://github.com/vergecurrency/VERGE
cd VERGE
sudo sh autogen.sh
chmod 777 ~/VERGE/share/genbuild.sh
chmod 777 ~/VERGE/src/leveldb/build_detect_platform



grep --include=*.hpp -r '/usr/' -e "define BOOST_LIB_VERSION"

sudo rm wrd01.txt
sudo rm wrd00.txt
sudo rm words
find /usr/ -name libboost_chrono.so > words
split -dl 1 --additional-suffix=.txt words wrd



if [ -e wrd01.txt ]; then
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

if [ -d /usr/local/BerkeleyDB.4.8/include ]
then
./configure CPPFLAGS="-I/usr/local/BerkeleyDB.4.8/include -O2" LDFLAGS="-L/usr/local/BerkeleyDB.4.8/lib" --with-gui=qt5 --with-boost-libdir=$(dirname "$(cat wrd0$answer.txt)")
echo "Using Berkeley Generic..."
else
./configure --with-gui=qt5 --with-boost-libdir=$(dirname "$(cat wrd0$answer.txt)")
echo "Using default system Berkeley..."
fi

make -j$(nproc)

if [ -e ~/VERGE/src/qt/VERGE-qt ]; then
#sudo apt-get -y install pulseaudio
#sudo apt-get -y install portaudio19-dev
# synthetic voice 
#cd ~
#wget https://sourceforge.net/projects/espeak/files/espeak/espeak-1.48/espeak-1.48.04-source.zip/download
#unzip -o download
#cd espeak-1.48.04-source/src
#cp portaudio19.h portaudio.h
#make
#cd ~
for i in 528 1000 1600 2000 3000
do
pactl load-module module-sine frequency=$i > /dev/null
sleep 0.1
done
sleep 1
pactl unload-module module-sine
pactl upload-sample complet test2
openssl rand -hex 4096 | padsp tee /dev/audio > /dev/null
sleep 1.3
pactl play-sample test2
#espeak mission,complete
sudo strip ~/VERGE/src/VERGEd
sudo strip ~/VERGE/src/qt/VERGE-qt
sudo make install
else
echo "Compile fail not VERGE-qt present"
fi

cd ~

#// Create the config file with random user and password

mkdir -p ~/.VERGE
if [ -e ~/.VERGE/VERGE.conf ]; then
    cp -a ~/.VERGE/VERGE.conf ~/.VERGE/VERGE.bak
fi
echo "rpcuser="$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 26 ; echo '') '\n'"rpcpassword="$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 26 ; echo '') '\n'"rpcport=20102" '\n'"port=21102" '\n'"daemon=1" '\n'"listen=1" '\n'"server=1" '\n'"addnode=103.18.40.125:21102" '\n'"addnode=104.131.144.82:21102" '\n'"addnode=138.197.68.130:21102" '\n'"addnode=144.76.167.66:21102" '\n'"addnode=152.186.36.86:21102" '\n'"addnode=159.203.121.202:21102" '\n'"addnode=172.104.157.38:21102" '\n'"addnode=192.99.7.127:21102" '\n'"addnode=219.89.84.46:21102" '\n'"addnode=45.32.129.168:21102" '\n'"addnode=45.55.59.206:21102" '\n'"addnode=46.4.64.68:21102" '\n'"addnode=51.15.46.1:21102" '\n'"addnode=52.9.109.214:21102" '\n'"addnode=66.55.64.183:21102" '\n'"addnode=67.167.207.164:21102" '\n'"addnode=78.46.190.152:21102"> ~/.VERGE/VERGE.conf

#// Extract http link, download blockchain and install it.

echo -n "Do you wish to download the complete VERGE Blockchain (y/n)?"
read answer
if echo "$answer" | grep -iq "^y" ;then
    sudo rm go.sh-Verge-Blockchain.zip

    wget --no-check-certificate https://verge-blockchain.com/blockchain/go.sh-Verge-Blockchain.zip

    #checksum
    sudo rm blockchain
    wget https://www.vergecurrency.com/checksums/blockchain
    md5sum go.sh-Verge-Blockchain.zip > md5
    checksum="$(grep $(cat md5) blockchain)"
    if [ -z "$checksum" ];
    then
    echo "Warning: MD5 is not matching"
    else
    echo "MD5 is matching...Success"
    fi

    unzip -o go.sh-Verge-Blockchain.zip -d ~/.VERGE
    sudo rm go.sh-Verge-Blockchain.zip
else
 echo "Blockchain will not be installed sync may be long"
fi

# Create Icon on Desktop and in menu
mkdir -p ~/Desktop/
sudo cp ~/VERGE/src/qt/res/icons/verge.png /usr/share/icons/
echo '#!/usr/bin/env xdg-open''\n'"[Desktop Entry]"'\n'"Version=1.0"'\n'"Type=Application"'\n'"Terminal=false"'\n'"Icon[en]=/usr/share/icons/verge.png"'\n'"Name[en]=VERGE"'\n'"Exec=VERGE-qt"'\n'"Name=VERGE"'\n'"Icon=/usr/share/icons/verge.png"'\n'"Categories=Network;Internet;" > ~/Desktop/VERGE.desktop
sudo chmod +x ~/Desktop/VERGE.desktop
sudo cp ~/Desktop/VERGE.desktop /usr/share/applications/VERGE.desktop
sudo chmod +x /usr/share/applications/VERGE.desktop

# Erase all VERGE compilation directory , cleaning

cd ~
#sudo rm -Rf ~/VERGE

#// Start Verge

VERGE-qt
if [ -e ~/.VERGE/wallet.dat ]; then
    cp ~/.VERGE/wallet.dat ~/vergewallet.bak
fi
