#!/bin/bash
#// full deployement : run ./go.sh

VERGEPWD=$PWD
DATE="_$(date +%d-%m-%Y)"
DISTR=$(lsb_release -si)
IFSWAP=$(cat /proc/swaps | wc -l)
#If zips location is changed, just modify these links accordingly
VERGE_BLOCKCHAIN_ZIPS_LOCATION="https://verge-blockchain.com"
VERGE_BLOCKCHAIN_ZIP_00="https://verge-blockchain*.*zip"
VERGE_BLOCKCHAIN_ZIP_01="https://s1.verge-blockchain*.*zip"
#If torrent location is changed, just modify these links accordingly
VERGE_BLOCKCHAIN_TORRENT_LOCATION="https://verge-blockchain.com/torrent1"
VERGE_BLOCKCHAIN_TORRENT_00="https://verge-blockchain*.*zip.torrent"

<<EOF 
	If you want to ensure "unpredictability at system startup" (provided that start-ups do not 
	involve much interaction with a human operator) - these parts
	need to be put in an appropriate start-up and shut-down scripts of your server:
	1.
		echo "Initializing random number generator..."
		random_seed=/var/run/random-seed
		# Carry a random seed from start-up to start-up
		# Load and then save the whole entropy pool
		if [ -f $random_seed ]; then
			cat $random_seed >/dev/urandom
		else
			touch $random_seed
		fi
		chmod 600 $random_seed
		dd if=/dev/urandom of=$random_seed count=1 bs=512
		and the following lines in an appropriate script which is run as
		the system is shutdown:
	2.
		# Carry a random seed from shut-down to start-up
		# Save the whole entropy pool
		echo "Saving random seed..."
		random_seed=/var/run/random-seed
		touch $random_seed
		chmod 600 $random_seed
		dd if=/dev/urandom of=$random_seed count=1 bs=512
EOF

# Create a swap file
if [ $IFSWAP -gt 1 ]; then
printf "\nSwap exists.\n"
else
	printf "\nAdding temporary Swap...\n"
	sudo dd if=/dev/zero of=/swapfile bs=1M count=1024
	sudo chmod 600 /swapfile
	sudo mkswap /swapfile
	sudo swapon /swapfile
fi

# Install dependencies
# Also, feel free to append "add-bitcoin-repository-COMMAND-LIST" for others distributions
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

sudo apt-get update && sudo apt-get -y install git libcanberra-gtk-module lynx xdg-utils unzip libdb4.8-dev libdb4.8++-dev build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils git libboost-all-dev libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 libevent-dev qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev libseccomp-dev libcap-dev
rc=$?

if [ $rc != 0 ]; then
	printf "\nFailed to install dependencies.\n"
	sleep 2
fi

# Check if boost (>= 1.63 version) has been already installed
printf "\nChecking boost...\n"
BOOST_VERSION=$(grep --include=*.hpp -r '/usr/include' -e "define BOOST_LIB_VERSION" | sed 's/[^0-9]//g')
BOOST_LIB=$(find /usr/lib -name libboost_chrono.so | wc -l)
if [ $BOOST_LIB -gt 0 ] && [ $BOOST_VERSION -ge 163 ]; then
	BOOST_ARG="/usr/lib/"
	printf "\nWill use installed boost\n"
	sleep 2
else
	# Check if boost (>= 1.63 version) has been already compiled
	BOOST_VERSION=$(grep --include=*.hpp -r '/usr/local/include' -e "define BOOST_LIB_VERSION" | sed 's/[^0-9]//g')
	BOOST_LIB=$(find /usr/local -name libboost_chrono.so | wc -l)
	if [ $BOOST_LIB -gt 0 ] && [ $BOOST_VERSION -ge 163 ]; then
		printf "\nWill use compiled boost\n"
		sleep 2
	else
		# Check if boost_1_63_0.zip has been already downloaded
		BOOST=$(find $HOME -name boost_1_63_0.zip | head -n 1)
		if [ -z $BOOST ]; then
			wget -O $HOME/boost_1_63_0.zip https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.zip
			rc=$?
			if [ $rc != 0 ]; then
				printf "\nFailed to get Boost-1.63.0 from https://sourceforge.net/projects/boost/files/boost/1.63.0/boost_1_63_0.zip\n"
				exit $rc
			fi
			printf "\nWill compile Boost-1.63...\n"
			sleep 2
			BOOST=$HOME/boost_1_63_0.zip
		fi
		unzip $BOOST
		cd boost_1_63_0
		sh bootstrap.sh
		sudo ./b2 install
		sudo ldconfig
		cd $VERGEPWD
		rm -rf boost_1_63_0
	fi
	BOOST_ARG="/usr/local/lib/"
fi


# Check if BerkeleyDb 4.8 has been already installed
printf "\nChecking BerkeleyDB...\n"
BERKELEY_LIB=$(find /usr/lib -name libdb_cxx-4.8.so | wc -l)
if [ $BERKELEY_LIB -gt 0 ]; then
	BERKELEY_ARG="/usr"
	printf "\nWill use installed BerkeleyDB-4.8\n"
	sleep 2
else
	# Check if BerkeleyDb 4.8 has been already compiled
	BERKELEY_LIB=$(find /usr/local/BerkeleyDB.4.8/lib/ -name libdb_cxx-4.8.so | wc -l)
	if [ $BERKELEY_LIB -gt 0 ]; then
		printf "\nWill use compiled BerkeleyDB-4.8\n"
		sleep 2
	else
		# Check if berkeley-db-4.8.30.NC.tar.gz has been already downloaded
		BERKELEYDB=$(find $HOME -name db-4.8.30.NC.tar.gz | head -n 1)
		if [ -z $BERKELEYDB ]; then
			wget -O $HOME/db-4.8.30.NC.tar.gz https://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz
			rc=$?
			if [ $rc != 0 ]; then
				printf "\nFailed to get BerkeleyDB-4.8 from https://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz\n"
				exit $rc
			fi
			printf "\nWill compile BerkeleyDB-4.8...\n"
			sleep 2
			BERKELEYDB=$HOME/db-4.8.30.NC.tar.gz
		fi
		tar -xzvf $BERKELEYDB
		cd db-4.8.30.NC/build_unix
		../dist/configure --enable-cxx
		make
		sudo make install
		cd $VERGEPWD
		rm -rf db-4.8.30.NC
	fi
	BERKELEY_ARG="/usr/local/BerkeleyDB.4.8"
fi

printf "\n"
./autogen.sh
#chmod 777 ~/VERGE/share/genbuild.sh
#chmod 777 ~/VERGE/src/leveldb/build_detect_platform
./configure CPPFLAGS="-I$BERKELEY_ARG/include -O2" LDFLAGS="-L$BERKELEY_ARG/lib" --with-gui=qt5 --with-boost-libdir=$BOOST_ARG

#https://github.com/google/leveldb/issues/556
#make -j$(nproc)

make
rc=$?
if [ -e /swapfile ]; then
	sudo swapoff /swapfile
	sudo rm -f /swapfile
fi

if [ $rc != 0 ]; then
	printf "\nCompilation failed, now cleaning...\n"
	sleep 2
	make clean
	exit $rc
else
	printf "\nCompilation succeeded, now installing...\n"
	sleep 2
#	sudo strip src/VERGEd
#	sudo strip src/qt/VERGE-qt
	sudo make install
	# Create Menu icon
	printf "[Desktop Entry]\nType=Application\nName=VERGE\nGenericName=VERGE Qt Wallet\nIcon=/usr/share/icons/verge.png\nCategories=Network;Internet;\nExec=VERGE-qt\nTerminal=false" > $HOME/Desktop/VERGE.desktop
	sudo cp -f src/qt/res/icons/verge.png /usr/share/icons/
	sudo cp -f $HOME/Desktop/VERGE.desktop /usr/share/applications/

	#// Create the config file with random user and password
	mkdir -p $HOME/.VERGE
	if [ -e $HOME/.VERGE/VERGE.conf ]; then
		cp -af $HOME/.VERGE/VERGE.conf $HOME/.VERGE/VERGE.bak
	fi
	printf "rpcuser=$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 26)\nrpcpassword=$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 26)\nrpcport=20102\nport=21102\ndaemon=1\nlisten=1\nserver=1\n" > $HOME/.VERGE/VERGE.conf
#	printf "\naddnode=103.18.40.125:21102\naddnode=104.131.144.82:21102\naddnode=138.197.68.130:21102\naddnode=144.76.167.66:21102\naddnode=152.186.36.86:21102\naddnode=159.203.121.202:21102\naddnode=172.104.157.38:21102\naddnode=192.99.7.127:21102\naddnode=219.89.84.46:21102\n	addnode=45.32.129.168:21102\naddnode=45.55.59.206:21102\naddnode=46.4.64.68:21102\naddnode=51.15.46.1:21102\naddnode=52.9.109.214:21102\naddnode=66.55.64.183:21102\naddnode=67.167.207.164:21102\naddnode=78.46.190.152:21102\n">> $HOME/.VERGE/VERGE.conf
	printf "\n"
	read -p "Do you want to download current VERGE Blockchain, y/n? (default: n) " CONFIRM
	if [ "$CONFIRM" = "y" ]; then
		printf "\n1 - zip file"
		printf "\n2 - torrent file\n"
		read CONFIRM
		if [ "$CONFIRM" = "1" ]; then
			rm -f $HOME/.VERGE/go.sh-Verge-Blockchain*.zip
			#Ignore all notifications about self-signed/letsencrypt certificates for lynx
			printf "FORCE_SSL_PROMPT:yes\n">lynx.cfg
			wget --no-check-certificate -O $HOME/.VERGE/go.sh-Verge-Blockchain$DATE.zip $(lynx -cfg=lynx.cfg -dump -listonly $VERGE_BLOCKCHAIN_ZIPS_LOCATION | grep -o $VERGE_BLOCKCHAIN_ZIP_00)
			rc=$?
			if [ $rc != 0 ]; then
				wget --no-check-certificate -O $HOME/.VERGE/go.sh-Verge-Blockchain$DATE.zip $(lynx -cfg=lynx.cfg -dump -listonly $VERGE_BLOCKCHAIN_ZIPS_LOCATION | grep -o $VERGE_BLOCKCHAIN_ZIP_01)

			fi
			if [ -e go.sh-Verge-Blockchain*.zip]; then
				unzip $HOME/.VERGE/go.sh-Verge-Blockchain*.zip -d $HOME/.VERGE
				rm go.sh-Verge-Blockchain*.zip
			else
				printf "\nCan't download VERGE Blockchain zip\n"
			fi
		fi
		if [ "$CONFIRM" = "2" ]; then
			xdg-open 2>/dev/null 1>&2 $(lynx -cfg=lynx.cfg -dump -listonly $VERGE_BLOCKCHAIN_TORRENT_LOCATION | grep -o $VERGE_BLOCKCHAIN_TORRENT_00)
			rc=$?
			if [ $rc != 0 ]; then
				printf "\nCan't download VERGE Blockchain torrent\n"
			fi
		fi
	fi
	printf "\n"
	read -p "Do you want to launch VERGE-qt wallet, y/n? (default: n) " CONFIRM
	if [ "$CONFIRM" = "y" ]; then
		VERGE-qt
	else
		exit 0
	fi
fi
