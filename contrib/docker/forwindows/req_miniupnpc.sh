#!/bin/bash

set -e

echo "=== Building MINIUPNPC now..."
cd /tmp
wget 'http://miniupnp.tuxfamily.org/files/download.php?file=miniupnpc-2.0.20180203.tar.gz' -O miniupnpc-2.0.20180203.tar.gz
tar xzf miniupnpc-2.0.20180203.tar.gz
cd /tmp/miniupnpc-2.0.20180203

#sed -i 's/CC = gcc/CC = i686-w64-mingw32.static-gcc/' Makefile.mingw

sed -i 's/wingenminiupnpcstrings \$/wine \.\/wingenminiupnpcstrings \$/' Makefile.mingw

if [ "$TARGET_PLATFORM" == "win64" ]
then
  echo "The target platform is win64"
  sed -i 's/dllwrap/x86_64-w64-mingw32-dllwrap/' Makefile.mingw
elif [ "$TARGET_PLATFORM" == "win32" ]
then
  echo "The target platform is win32"
  sed -i 's/dllwrap/i686-w64-mingw32-dllwrap/' Makefile.mingw
else
  echo "You should setup TARGET_PLATFORM environment variable. It can be win64 or win32"
  exit 1
fi

sed -i 's/wine /.\/updateminiupnpcstrings.sh #/' Makefile.mingw
sed -i 's/-enable-stdcall-fixup/-Wl,-enable-stdcall-fixup/' Makefile.mingw
#sed -i 's/driver-name gcc/driver-name i686-w64-mingw32.static-gcc/' Makefile.mingw
sed -i 's/; miniupnpc library/miniupnpc/' miniupnpc.def
make -f Makefile.mingw upnpc-static libminiupnpc.a
cd /tmp
ln -s miniupnpc-2.0.20180203 miniupnpc

echo "=== Done building MINIUPNPC =="
