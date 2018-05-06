#!/bin/bash

# Debian Stretch, Debian Buster, Ubuntu 16.04

VERGEPWD=$PWD
DISTR=$(lsb_release -si)
IFSWAP=$(cat /proc/swaps | wc -l)

# Install dependencies
sudo apt-get -y install software-properties-common

case "$DISTR" in
	Debian)
		# https://launchpad.net/~bitcoin/+archive/ubuntu/bitcoin
		# No longer supports precise, due to its ancient gcc and Boost versions.
		sudo apt-get install dirmngr
		sudo add-apt-repository "deb http://ppa.launchpad.net/bitcoin/bitcoin/ubuntu trusty main"
		sudo apt-get update >> /dev/null 2> /tmp/temp_apt_key.txt
		TEMP_APT_KEY=$(cat /tmp/temp_apt_key.txt | cut -d":" -f6 | cut -d" " -f3)
		sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys $TEMP_APT_KEY
		sudo rm -f /tmp/temp_apt_key.txt
		;;
	Ubuntu)
		sudo add-apt-repository ppa:bitcoin/bitcoin
		;;
esac

sudo apt-get update && sudo apt-get -y install git libcanberra-gtk-module lynx unzip libdb4.8-dev libdb4.8++-dev build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils git libboost-all-dev libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 libevent-dev qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev libseccomp-dev libcap-dev
rc=$?

if [ $rc != 0 ]; then
	printf "\nFailed to install dependencies\n"
	exit $rc
else
	# Create a swap file
	if [ $IFSWAP -gt 1 ]; then
		printf "\nSwap exists\n"
		sleep 2
	else
		printf "\nAdding temporary Swap\n"
		sudo dd if=/dev/zero of=/swapfile bs=1M count=1024
		sudo chmod 600 /swapfile
		sudo mkswap /swapfile
		sudo swapon /swapfile
	fi	
	if [ -e ~/.VERGE/wallet.dat ]; then
		cp ~/.VERGE/wallet.dat ~/vergewallet.dat.back
	fi	
	if [ -d VERGE ]; then
		sudo rm -rf VERGE
	fi	
	git clone --recurse-submodules https://github.com/vergecurrency/VERGE
	cd VERGE
	# Check if there is BerkeleyDb 4.8
	BERKELEY_LIB=$(find /usr/lib -name libdb_cxx-4.8.so | wc -l)
	if [ $BERKELEY_LIB -gt 0 ]; then
	printf "\nBerkeleyDB-4.8 is already installed\n"
		sleep 2
	else
		printf "\nWill compile BerkeleyDB-4.8\n"
		sleep 2
		wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz
		tar -xzvf db-4.8.30.NC.tar.gz
		cd db-4.8.30.NC/build_unix
		../dist/configure --enable-cxx
		make
		sudo make install
		sudo ln -s /usr/local/BerkeleyDB.4.8/lib/libdb-4.8.so /usr/lib/libdb-4.8.so
		sudo ln -s /usr/local/BerkeleyDB.4.8/lib/libdb_cxx-4.8.so /usr/lib/libdb_cxx-4.8.so
		cd $VERGEPWD/VERGE
	fi
	# Check if boost has been installed, and if it is >= 1.63 version
	printf "\nChecking boost\n"
	BOOST_VERSION=$(grep --include=*.hpp -r '/usr/include' -e "define BOOST_LIB_VERSION" | sed 's/[^0-9]//g')
	BOOST_LIB=$(find /usr/lib -name libboost_chrono.so | wc -l)
	if [ $BOOST_LIB -gt 0 ] && [ $BOOST_VERSION -ge 163 ]; then
		printf "\nWill use repository boost\n"
		sleep 2
		BOOST_ARG="/usr/lib/"
	else
		#Check if libboost has been previously compiled and if it is >= 1.63
		BOOST_VERSION=$(grep --include=*.hpp -r '/usr/local/include' -e "define BOOST_LIB_VERSION" | sed 's/[^0-9]//g')
		BOOST_LIB=$(find /usr/local -name libboost_chrono.so | wc -l)
		if [ $BOOST_LIB -gt 0 ] && [ $BOOST_VERSION -ge 163 ]; then
			printf "\nWill use previously compiled boost\n"
			sleep 2
		else
			printf "\nWill compile Boost-1.63\n"
			sleep 2
			wget https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.zip
			unzip -o boost_1_63_0.zip
			cd boost_1_63_0
			sh bootstrap.sh
			sudo ./b2 install
			sudo ldconfig
			cd $VERGEPWD/VERGE
		fi
		BOOST_ARG="/usr/local/lib/"
	fi

	./autogen.sh
	./configure CPPFLAGS="-I/usr/local/BerkeleyDB.4.8/include -O2" LDFLAGS="-L/usr/local/BerkeleyDB.4.8/lib" --with-gui=qt5 --with-boost-libdir=$BOOST_ARG
	make -j$(nproc)
	sudo make install

	# Create Desktop icon and Menu icon
	printf "[Desktop Entry]\nType=Application\nName=VERGE\nGenericName=VERGE Qt Wallet\nIcon=/usr/share/icons/verge.png\nCategories=Network;Internet;\nExec=VERGE-qt\nTerminal=false" > ~/Desktop/verge.desktop
	chmod +x ~/Desktop/verge.desktop
	sudo cp src/qt/res/icons/verge.png /usr/share/icons/
	sudo cp ~/Desktop/verge.desktop /usr/share/applications/

	sudo swapoff /swapfile
	sudo rm -f /swapfile

	cd $VERGEPWD
	sudo rm -Rf VERGE
fi
