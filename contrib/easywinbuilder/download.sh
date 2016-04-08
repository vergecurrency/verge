set -o errexit

echo Creating lib directory...
cd $ROOTPATHSH
if [ ! -d $EWBLIBS ]; then
    mkdir $EWBLIBS
fi

# echo Downloading source...
# wget --no-check-certificate -N "https://github.com/phelixbtc/namecoin-qt/archive/easywinbuilder.tar.gz" -O "source.tar.gz"
# echo

echo Downloading dependencies...
cd libs
wget -N "http://www.openssl.org/source/$OPENSSL.tar.gz"
wget -N "http://download.oracle.com/berkeley-db/$BERKELEYDB.tar.gz"
wget -N "http://downloads.sourceforge.net/project/boost/boost/$BOOSTVERSION/$BOOST.tar.gz"
wget -N "http://miniupnp.tuxfamily.org/files/download.php?file=$MINIUPNPC.tar.gz"
echo
