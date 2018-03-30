#!/bin/bash

usage () {
	echo "USAGE: build-verge.sh [-v version] [-s SIGNER] [-d true/false]"
	echo "Where v(ersion) should be the next version of the wallet"
	echo "Where s(igner) is used for the gitian signature, for example drakandar@VERGEcurrency.com - no signer will skip signatures"
	echo "Where d(ownload) is used to determine if you want to download third party libraries. Defaults to TRUE"
	exit 1
}


# check on options
parseopts () {
	while getopts ":v:s:d:" optname
	do
    	case $optname in
    		v)
				VERSION=$OPTARG
				;;
    		s)
				SIGNER=$OPTARG	
				;;
    		d)
				DOWNLOAD=$OPTARG	
				;;
								
    		\?)
				echo "Unknown option $OPTARG"
				;;
    		:)
				echo "No argument value for option $OPTARG"
				;;
    		*)
      			# Should not occur
				echo "Unknown error while processing options"
				;;
    	esac
	done  
}

# check on options
	parseopts "$@"

# output input vars
	echo "----------------------------------INPUT-VARS----------------------------------------"
	echo " Input:"
	echo "  VERSION=${VERSION}"
	echo "  SIGNER=${SIGNER}"		
	echo ""
	echo " Environment (Required):"
	echo "  USER=${USER}"
	echo ""
	echo " Environment (Optional):"	
	echo "-------------------------------END-INPUT-VARS---------------------------------------"

# validate input vars

	if [ -z "$VERSION" ]
	then
		echo "No VERSION defined, exiting...."
		exit 3
	fi
	
	if [ -z "$USER" ]
	then
		echo "No USER defined (impossibru!), exiting...."
		exit 3
	fi	

#Parse dynvars	
			
	if [ -z "$DOWNLOAD" ]
	then
		DOWNLOAD=true
	fi	

#Output dynvars
	echo "----------------------------------DYNAMIC-VARS----------------------------------------"
	echo "  DOWNLOAD=${DOWNLOAD}"
	echo "-------------------------------END-DYNAMIC-VARS---------------------------------------"

# Make sure packages are installed
sudo apt-get install apache2 git apt-cacher-ng python-vm-builder qemu-kvm ruby git build-essential dh-autoreconf

# Prepare working dir
sudo mkdir -p /data/coins
sudo chown -R $USER.$USER /data/coins
cd /data/coins

# Clone sources
git clone git://github.com/VERGETeam/fair-coin.git verge
git clone git://github.com/devrandom/gitian-builder.git

#Mk dirs for inputs and download them
mkdir gitian-builder/inputs
cd gitian-builder/inputs

if $DOWNLOAD
then
	wget 'http://miniupnp.free.fr/files/download.php?file=miniupnpc-1.9.tar.gz' -O miniupnpc-1.9.tar.gz
	wget 'https://www.openssl.org/source/openssl-1.1.0g.tar.gz'
	wget 'http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz'
	wget 'http://zlib.net/fossils/zlib-1.2.8.tar.gz'
	wget 'ftp://ftp.simplesystems.org/pub/png/src/history/libpng16/libpng-1.6.8.tar.gz'
	wget 'https://fukuchi.org/works/qrencode/qrencode-3.4.3.tar.bz2'
	wget 'https://downloads.sourceforge.net/project/boost/boost/1.55.0/boost_1_55_0.tar.bz2'
	wget 'https://svn.boost.org/trac/boost/raw-attachment/ticket/7262/boost-mingw.patch' -O boost-mingw-gas-cross-compile-2013-03-03.patch
	wget 'https://download.qt-project.org/official_releases/qt/4.8/4.8.5/qt-everywhere-opensource-src-4.8.5.tar.gz'
	wget 'https://download.qt-project.org/archive/qt/4.6/qt-everywhere-opensource-src-4.6.4.tar.gz'
	wget 'https://protobuf.googlecode.com/files/protobuf-2.5.0.tar.bz2'
fi

#Build gitian base vm's
cd ..
bin/make-base-vm --suite precise --arch i386
bin/make-base-vm --suite precise --arch amd64

#Incremental build gitian descriptors
bin/gbuild ../verge/contrib/gitian-descriptors/boost-linux.yml
mv build/out/boost-*.zip inputs/

bin/gbuild ../verge/contrib/gitian-descriptors/deps-linux.yml
mv build/out/verge-deps-*.zip inputs/

bin/gbuild ../verge/contrib/gitian-descriptors/qt-linux.yml
mv build/out/qt-*.tar.gz inputs/

bin/gbuild ../verge/contrib/gitian-descriptors/boost-win.yml
mv build/out/boost-*.zip inputs/

bin/gbuild ../verge/contrib/gitian-descriptors/deps-win.yml
mv build/out/verge-deps-*.zip inputs/

bin/gbuild ../verge/contrib/gitian-descriptors/qt-win.yml
mv build/out/qt-*.zip inputs/

bin/gbuild ../verge/contrib/gitian-descriptors/protobuf-win.yml
mv build/out/protobuf-*.zip inputs/

#Check if there is a signer, if not exit.
if [ -z "$SIGNER" ]
then
	echo "No SIGNER defined, exiting...."
	exit 0
fi

# Clone gitian signatures and append to them
cd ..
git clone https://github.com/VERGETeam/gitian.sigs.git

cd gitian-builder
bin/gbuild --commit fair-coin=v${VERSION} ../fair-coin/contrib/gitian-descriptors/gitian-linux.yml
bin/gsign --signer $SIGNER --release ${VERSION} --destination ../gitian.sigs/ ../fair-coin/contrib/gitian-descriptors/gitian-linux.yml
bin/gbuild --commit fair-coin=v${VERSION} ../fair-coin/contrib/gitian-descriptors/gitian-win.yml
bin/gsign --signer $SIGNER --release ${VERSION}-win --destination ../gitian.sigs/ ../fair-coin/contrib/gitian-descriptors/gitian-win.yml

#TODO: Rename the directory in /data/coins/gitian.sigs/1.0.0[-win] to your github username.
