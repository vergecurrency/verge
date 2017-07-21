#// full deployement : run sh go.sh
cd ~
sudo dd if=/dev/zero of=/swapfile1 bs=1024 count=524288
sudo mkswap /swapfile1
sudo chown root:root /swapfile1
sudo chmod 0600 /swapfile1
sudo swapon /swapfile1

sudo apt-get -y install software-properties-common

sudo add-apt-repository ppa:bitcoin/bitcoin

sudo apt-get update

sudo apt-get install libcanberra-gtk-module

sudo apt-get -y install git libdb4.8-dev libdb4.8++-dev

sudo apt-get -y install git build-essential libtool autotools-dev autoconf automake pkg-config libssl-dev libevent-dev bsdmainutils git libprotobuf-dev protobuf-compiler libqrencode-dev

sudo apt-get -y install libqt5gui5 libqt5core5a libqt5webkit5-dev libqt5dbus5 qttools5-dev qttools5-dev-tools

sudo apt-get -y install libminiupnpc-dev

sudo apt-get -y install libboost-all-dev

sudo apt-get -y install --no-install-recommends gnome-panel

sudo apt-get -y install lynx

sudo apt-get -y install unzip

cd ~

#// Compile Berkeley
if [ -e /usr/lib/libdb_cxx-4.8.so ]
then
echo "BerkeleyDb already present"
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
fi

#// Clone files from repo, Permissions and make

git clone https://github.com/vergecurrency/VERGE
cd VERGE
sudo sh autogen.sh
chmod 777 ~/VERGE/share/genbuild.sh
chmod 777 ~/VERGE/src/leveldb/build_detect_platform
cd ~/raspi 
./configure CPPFLAGS="-I/usr/local/BerkeleyDB.4.8/include -O2" LDFLAGS="-L/usr/local/BerkeleyDB.4.8/lib" --with-gui=qt5
make -j4
sudo strip ~/VERGE/src/VERGEd
sudo strip ~/VERGE/src/qt/VERGE-qt
sudo make install
cd ~

#// Create the config file with random user and password

mkdir ~/.VERGE
echo "rpcuser="$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 26 ; echo '') '\n'"rpcpassword="$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 26 ; echo '') '\n'"rpcport=20102" '\n'"port=21102" '\n'"daemon=1" '\n'"listen=1" > ~/.VERGE/VERGE.conf

#// Extract http link, download blockchain and install it.

until [ -e Verge*.zip ]
do
echo "wget" $(lynx --dump --listonly http://vergecurrency.de | grep -o "http:*.*zip") > link.sh
sh link.sh
done

unzip -o Verge-Blockchain*.zip -d ~/.VERGE
sudo rm Verge-Blockchain*.zip

# Create Icon on Desktop and in menu

sudo cp ~/VERGE/src/qt/res/icons/verge.png /usr/share/icons/
echo '#!/usr/bin/env xdg-open''\n'"[Desktop Entry]"'\n'"Version=1.0"'\n'"Type=Application"'\n'"Terminal=false"'\n'"Icon[en]=/usr/share/icons/verge.png"'\n'"Name[en]=VERGE"'\n'"Exec=VERGE-qt"'\n'"Name=VERGE"'\n'"Icon=/usr/share/icons/verge.png"'\n'"Categories=Network;Internet;" > ~/Desktop/VERGE.desktop
sudo chmod +x ~/Desktop/VERGE.desktop
sudo cp ~/Desktop/VERGE.desktop /usr/share/applications/VERGE.desktop
sudo chmod +x /usr/share/applications/VERGE.desktop

# Erase all VERGE compilation directory , cleaning

cd ~
sudo rm -Rf ~/VERGE

#// Start Verge

VERGE-qt
