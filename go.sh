#!/bin/bash
#// full deployement : run ./go.sh

VERGEPWD=$PWD
DISTR=$(lsb_release -si)
IFSWAP=$(cat /proc/swaps | wc -l)
#If zips location is changed, just modify these links accordingly
VERGE_BLOCKCHAIN_ZIP_LOCATION_00="https://verge-blockchain.com"
VERGE_BLOCKCHAIN_ZIP_LOCATION_01="https://s1.verge-blockchain.com"
VERGE_BLOCKCHAIN_ZIP_REGEXP_00="https://verge-blockchain*.*zip"
VERGE_BLOCKCHAIN_ZIP_REGEXP_01="https://s1.verge-blockchain*.*zip"
#If torrent location is changed, just modify these links accordingly
VERGE_BLOCKCHAIN_TORRENT_LOCATION="https://verge-blockchain.com/torrent1"
VERGE_BLOCKCHAIN_TORRENT_REGEXP="https://verge-blockchain*.*zip.torrent"

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
	sudo dd if=/dev/zero of=/temp_swap bs=1M count=1024
	sudo chmod 600 /temp_swap
	sudo mkswap /temp_swap
	sudo swapon /temp_swap
fi

# Install dependencies
sudo apt-get -y install software-properties-common
case "$DISTR" in
	Debian)
		# https://launchpad.net/~bitcoin/+archive/ubuntu/bitcoin
		# No longer supports precise, due to its ancient gcc and Boost versions.
		sudo apt-get -y install dirmngr
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

sudo apt-get update && sudo apt-get -y install git libcanberra-gtk-module lynx xdg-utils unzip libdb4.8-dev libdb4.8++-dev build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils git libboost-all-dev libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev libseccomp-dev libcap-dev
rc=$?

if [ $rc != 0 ]; then
	printf "\nFailed to install dependencies.\n"
	sleep 2
fi

printf "\nChecking boost...\n"
BOOST_VERSION=$(grep --include=*.hpp -r '/usr/include' -e "define BOOST_LIB_VERSION" | sed 's/[^0-9]//g')
BOOST_LIB=$(find /usr/lib -name libboost_chrono.so)
if [ ! -z "$BOOST_LIB" ] && [ $BOOST_VERSION -ge 163 ]; then
	BOOST_ARG="/usr/lib/"
	printf "\nWill use installed boost\n"
	sleep 2
else
	BOOST_ARG="/usr/local/lib/"
	# Check if boost (>= 1.63 version) has been already compiled
	BOOST_VERSION=$(grep --include=*.hpp -r '/usr/local/include' -e "define BOOST_LIB_VERSION" | sed 's/[^0-9]//g')
	BOOST_LIB=$(find /usr/local/lib -name libboost_chrono.so)
	if [ ! -z "$BOOST_LIB" ] && [ $BOOST_VERSION -ge 163 ]; then
		printf "\nWill use compiled boost\n"
		sleep 2
	else
		# Check if boost_1_63_0.zip has been already downloaded
		BOOST=$(find $HOME -name boost_1_63_0.zip | head -n 1)
		if [ -z "$BOOST" ]; then
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
		./bootstrap.sh
		sudo ./b2 install
		cd $VERGEPWD
		rm -rf boost_1_63_0
	fi
fi

printf "\nChecking BerkeleyDB...\n"
BERKELEY_LIB=$(find /usr/lib -name libdb_cxx-4.8.so)
if [ -e $BERKELEY_LIB ]; then
	if [ ! -L $BERKELEY_LIB ]; then
		# BerkeleyDB-4.8 has been successfully installed from Bitcoin repo's
		BERKELEY_ARG="/usr"
		printf "\nWill use installed BerkeleyDB-4.8\n"
		sleep 2
	else
		# BerkeleyDB-4.8 has been successfully compiled earlier
		BERKELEY_ARG="/usr/local/BerkeleyDB.4.8"
		printf "\nWill use compiled BerkeleyDB-4.8\n"
		sleep 2
	fi
else
	# There is no BerkeleyDB-4.8 compiled or installed
	BERKELEY_ARG="/usr/local/BerkeleyDB.4.8"
	printf "\nWill compile BerkeleyDB-4.8...\n"
	sleep 2
	# Check if berkeley-db-4.8.30.NC.tar.gz has been already downloaded
	BERKELEYDB=$(find $HOME -name db-4.8.30.NC.tar.gz | head -n 1)
	if [ -z "$BERKELEYDB" ]; then
		wget -O $HOME/db-4.8.30.NC.tar.gz https://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz
		rc=$?
		if [ $rc != 0 ]; then
			printf "\nFailed to get BerkeleyDB-4.8 from https://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz\n"
			exit $rc
		fi
		sleep 2
		BERKELEYDB=$HOME/db-4.8.30.NC.tar.gz
	fi
	tar -xzvf $BERKELEYDB
	# Fixing "atomic.h.. warning: conflicting types for built-in function" error during compilation
	sudo sed -i 's/__atomic_compare_exchange(/__atomic_compare_exchange_db(/g' db-4.8.30.NC/dbinc/atomic.h
	cd db-4.8.30.NC/build_unix
	../dist/configure --enable-cxx
	make
	sudo make install
	sudo ln -sf /usr/local/BerkeleyDB.4.8/lib/libdb-4.8.so /usr/lib/libdb-4.8.so
	sudo ln -sf /usr/local/BerkeleyDB.4.8/lib/libdb_cxx-4.8.so /usr/lib/libdb_cxx-4.8.so
	cd $VERGEPWD
	rm -rf db-4.8.30.NC
fi

printf "\n"
./autogen.sh
./configure CPPFLAGS="-I$BERKELEY_ARG/include -O2" LDFLAGS="-L$BERKELEY_ARG/lib" --with-gui=qt5 --with-boost-libdir=$BOOST_ARG

make
rc=$?
if [ -e /temp_swap ]; then
	sudo swapoff /temp_swap
	sudo rm -f /temp_swap
fi

if [ $rc != 0 ]; then
	printf "\nCompilation failed, now cleaning...\n"
	sleep 2
	make clean
	exit $rc
else
	printf "\nCompilation succeeded, now installing...\n"
	sleep 2
	sudo make install
	# Create Menu icon
	printf "[Desktop Entry]\nType=Application\nName=VERGE\nGenericName=VERGE Qt Wallet\nIcon=/usr/share/icons/verge.png\nCategories=Network;Internet;\nExec=VERGE-qt\nTerminal=false" > $HOME/VERGE/VERGE.desktop
	sudo cp -f src/qt/res/icons/verge.png /usr/share/icons/
	sudo cp -f $HOME/VERGE/VERGE.desktop /usr/share/applications/
	#// Create the config file with random user and password
	mkdir -p $HOME/.VERGE
	if [ -e $HOME/.VERGE/VERGE.conf ]; then
		cp -af $HOME/.VERGE/VERGE.conf $HOME/.VERGE/VERGE.conf.back
	fi
	printf "rpcuser=$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 26)\nrpcpassword=$(head /dev/urandom | tr -dc A-Za-z0-9 | head -c 26)\nrpcport=20102\nport=21102\ndaemon=1\nlisten=1\nserver=1\n" > $HOME/.VERGE/VERGE.conf
#	printf "\naddnode=103.18.40.125:21102\naddnode=104.131.144.82:21102\naddnode=138.197.68.130:21102\naddnode=144.76.167.66:21102\naddnode=152.186.36.86:21102\naddnode=159.203.121.202:21102\naddnode=172.104.157.38:21102\naddnode=192.99.7.127:21102\naddnode=219.89.84.46:21102\n	addnode=45.32.129.168:21102\naddnode=45.55.59.206:21102\naddnode=46.4.64.68:21102\naddnode=51.15.46.1:21102\naddnode=52.9.109.214:21102\naddnode=66.55.64.183:21102\naddnode=67.167.207.164:21102\naddnode=78.46.190.152:21102\n">> $HOME/.VERGE/VERGE.conf
	printf "\n"
	read -p "Do you want to download current VERGE Blockchain, y/n? (default: n) " CONFIRM
	if [ "$CONFIRM" = "y" ]; then
		printf "\n1 - zip file"
		printf "\n2 - torrent file\n\n"
		read CONFIRM
		if [ "$CONFIRM" = "1" ]; then
			# Temporarily ignore all notifications about self-signed/letsencrypt certificates for lynx. But better solution would be to fix the certificate problem.
			printf "FORCE_SSL_PROMPT:yes\n">lynx.cfg
			VERGE_BLOCKCHAIN_ZIP_NAME_00=$(lynx -cfg=lynx.cfg -dump -listonly $VERGE_BLOCKCHAIN_ZIP_LOCATION_00 | grep -o $VERGE_BLOCKCHAIN_ZIP_REGEXP_00 | sed 's/.*\///g')
			if [ -z "$VERGE_BLOCKCHAIN_ZIP_NAME_00" ]; then
				printf "\nCan't download VERGE Blockchain zip from $VERGE_BLOCKCHAIN_ZIP_LOCATION_00, will try $VERGE_BLOCKCHAIN_ZIP_LOCATION_01\n"
				VERGE_BLOCKCHAIN_ZIP_NAME_01=$(lynx -cfg=lynx.cfg -dump -listonly $VERGE_BLOCKCHAIN_ZIP_LOCATION_01 | grep -o $VERGE_BLOCKCHAIN_ZIP_REGEXP_01 | sed 's/.*\///g')
				if [ -z "$VERGE_BLOCKCHAIN_ZIP_NAME_01" ]; then
					printf "\nCan't download VERGE Blockchain zip from $VERGE_BLOCKCHAIN_ZIP_LOCATION_01\n"
				else
					CURRENT_BLOCKCHAIN_ZIP_NAME=$VERGE_BLOCKCHAIN_ZIP_NAME_01
					CURRENT_BLOCKCHAIN_ZIP_LOCATION=$VERGE_BLOCKCHAIN_ZIP_LOCATION_01
					CURRENT_BLOCKCHAIN_ZIP_REGEXP=$VERGE_BLOCKCHAIN_ZIP_REGEXP_01
				fi
			else
				CURRENT_BLOCKCHAIN_ZIP_NAME=$VERGE_BLOCKCHAIN_ZIP_NAME_00
				CURRENT_BLOCKCHAIN_ZIP_LOCATION=$VERGE_BLOCKCHAIN_ZIP_LOCATION_00
				CURRENT_BLOCKCHAIN_ZIP_REGEXP=$VERGE_BLOCKCHAIN_ZIP_REGEXP_00
			fi
			if [ ! -z "$CURRENT_BLOCKCHAIN_ZIP_NAME" ]; then
				wget -c --no-check-certificate -O $HOME/.VERGE/$CURRENT_BLOCKCHAIN_ZIP_NAME $(lynx -cfg=lynx.cfg -dump -listonly $CURRENT_BLOCKCHAIN_ZIP_LOCATION | grep -o $CURRENT_BLOCKCHAIN_ZIP_REGEXP)
			fi
			if [ -e $HOME/.VERGE/$CURRENT_BLOCKCHAIN_ZIP_NAME ]; then
				unzip $HOME/.VERGE/$CURRENT_BLOCKCHAIN_ZIP_NAME -d $HOME/.VERGE
				rc=$?
				if [ $rc != 0 ]; then
					printf "\n$CURRENT_BLOCKCHAIN_ZIP_NAME download has not been completed\n"
				else
					rm -f $HOME/.VERGE/$CURRENT_BLOCKCHAIN_ZIP_NAME
				fi
			else
				printf "\n$CURRENT_BLOCKCHAIN_ZIP_NAME download was not successful\n"
			fi
		fi
		if [ "$CONFIRM" = "2" ]; then
			xdg-open 2>/dev/null 1>&2 $(lynx -cfg=lynx.cfg -dump -listonly $VERGE_BLOCKCHAIN_TORRENT_LOCATION | grep -o $VERGE_BLOCKCHAIN_TORRENT_REGEXP)
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
