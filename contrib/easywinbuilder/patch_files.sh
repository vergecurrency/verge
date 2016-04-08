sed -i.bak 's/INCLUDEPATHS= \\/INCLUDEPATHS?= \\/g' $ROOTPATHSH/src/makefile.mingw
sed -i.bak 's/LIBPATHS= \\/LIBPATHS?= \\/g' $ROOTPATHSH/src/makefile.mingw
sed -i.bak 's/USE_UPNP:=-/USE_UPNP?=-/g' $ROOTPATHSH/src/makefile.mingw

sed -i.bak 's,#include <miniupnpc/miniwget.h>,#include <miniwget.h>,g' $ROOTPATHSH/src/net.cpp
sed -i.bak 's,#include <miniupnpc/miniupnpc.h>,#include <miniupnpc.h>,g' $ROOTPATHSH/src/net.cpp
sed -i.bak 's,#include <miniupnpc/upnpcommands.h>,#include <upnpcommands.h>,g' $ROOTPATHSH/src/net.cpp
sed -i.bak 's,#include <miniupnpc/upnperrors.h>,#include <upnperrors.h>,g' $ROOTPATHSH/src/net.cpp

sed -i.bak 's/\$(CC) -enable-stdcall-fixup/\$(CC) -Wl,-enable-stdcall-fixup/g' $ROOTPATHSH/${EWBLIBS}/${MINIUPNPC}/Makefile.mingw  # workaround, see http://stackoverflow.com/questions/13227354/warning-cannot-find-entry-symbol-nable-stdcall-fixup-defaulting
sed -i.bak 's/all:	init upnpc-static upnpc-shared testminixml libminiupnpc.a miniupnpc.dll/all:	init upnpc-static/g' $ROOTPATHSH/${EWBLIBS}/${MINIUPNPC}/Makefile.mingw  # only need static, rest is not compiling

# += does not work on windows defined variables
sed -i.bak 's/CC = gcc/CC=gcc ${ADDITIONALCCFLAGS} -Wall/g' $ROOTPATHSH/${EWBLIBS}/${MINIUPNPC}/Makefile.mingw

sed -i.bak 's/CFLAGS=-mthreads/CFLAGS=${ADDITIONALCCFLAGS} -mthreads/g' $ROOTPATHSH/src/makefile.mingw

# winsock trouble
sed -i.bak 's/-DWIN32 -D_WINDOWS/-DWIN32 -DWIN32_LEAN_AND_MEAN -D_WINDOWS/g' $ROOTPATHSH/src/makefile.mingw
sed -i.bak 's/DEFINES += WIN32$/DEFINES += WIN32 WIN32_LEAN_AND_MEAN/g' $ROOTPATHSH/${COINNAME}-qt.pro

# https://github.com/bitcoin/bitcoin/pull/3121 multi threading fix
sed -i.bak 's/DEFS=-DWIN32/DEFS=-D_MT -DWIN32/g' $ROOTPATHSH/src/makefile.mingw
sed -i.bak 's/LIBS += -l kernel32/LIBS +=  -l mingwthrd -l kernel32/g' $ROOTPATHSH/src/makefile.mingw
