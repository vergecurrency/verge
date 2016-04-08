if [ -d "${ROOTPATHSH}/src/leveldb" ]; then
    echo leveldb...
    cd $ROOTPATHSH/src/leveldb
    make memenv_test TARGET_OS=OS_WINDOWS_CROSSCOMPILE \
     OPT="${ADDITIONALCCFLAGS}"
     if [ "$?"-ne 0]; then echo "LevelDB build failed."; read -n 1 -s; exit 1;fi
    echo
    cd ../../$EWBPATH
fi

cd $ROOTPATHSH/$EWBLIBS

echo db...
cd $BERKELEYDB
cd build_unix
../dist/configure --disable-replication --enable-mingw --enable-cxx \
 CXXFLAGS="${ADDITIONALCCFLAGS}" \
 CFLAGS="${ADDITIONALCCFLAGS}"
if [ ${?} -ne 0 ]; then echo "BerkeleyDB configure failed."; read -n 1 -s; exit 1;fi
sed -i 's/typedef pthread_t db_threadid_t;/typedef u_int32_t db_threadid_t;/g' db.h  # workaround, see https://bitcointalk.org/index.php?topic=45507.0
make
if [ ${?} -ne 0 ]; then echo "BerkeleyDB make failed."; read -n 1 -s; exit 1;fi
cd ..
cd ..
echo

echo  openssl...
cd $OPENSSL
export CC="gcc ${ADDITIONALCCFLAGS}"
./config
if [ ${?} -ne 0 ]; then echo "OpenSSL config failed."; read -n 1 -s; exit 1;fi
make
if [ ${?} -ne 0 ]; then echo "OpenSSL make failed."; read -n 1 -s; exit 1;fi
cd ..
echo

cd ../$EWBPATH