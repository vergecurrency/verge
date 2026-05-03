Windows Cross-Compile Build Instructions (Ubuntu 24.04)
========================================================

This document matches the Windows jobs in `.github/workflows/check-commit.yml`.

General prerequisites
---------------------

```shell
sudo apt-get update
sudo apt-get install -y \
  build-essential libtool gperf autotools-dev automake pkg-config \
  bsdmainutils curl git bison byacc python3 nsis ccache \
  ninja-build flex nodejs \
  libgl1-mesa-dev libegl1-mesa-dev libx11-dev libx11-xcb-dev \
  libxcb1-dev libxkbcommon-dev libxkbcommon-x11-dev
```

64-bit Windows (x86_64-w64-mingw32)
------------------------------------

Install toolchain:

```shell
sudo apt-get install -y g++-mingw-w64-x86-64 mingw-w64-x86-64-dev
sudo update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix
```

Build:

```shell
cd depends
make HOST=x86_64-w64-mingw32 -j2
cd ..

./autogen.sh

QT_HOST_PREFIX="$PWD/depends/x86_64-w64-mingw32/native/qt-host"
QT_HOST_BIN="$QT_HOST_PREFIX/bin"
mkdir -p "$PWD/.qt-tool-aliases"
ln -sf "$QT_HOST_BIN/moc" "$PWD/.qt-tool-aliases/moc-qt6"
ln -sf "$QT_HOST_BIN/uic" "$PWD/.qt-tool-aliases/uic-qt6"
ln -sf "$QT_HOST_BIN/rcc" "$PWD/.qt-tool-aliases/rcc-qt6"

export PATH="$PWD/.qt-tool-aliases:$QT_HOST_BIN:$QT_HOST_PREFIX/libexec:$PWD/depends/x86_64-w64-mingw32/native/bin:$PATH"

CONFIG_SITE="$PWD/depends/x86_64-w64-mingw32/share/config.site" \
./configure --enable-scrypt-sse2 --prefix=/ --disable-bench --disable-tests \
  --with-gui=qt6 \
  --with-qt-bindir="$QT_HOST_BIN"

make -j1
```

Optional strip:

```shell
cd src && x86_64-w64-mingw32-strip verged.exe verge-cli.exe verge-tx.exe
cd ../qt && x86_64-w64-mingw32-strip verge-qt.exe
```

64-bit Windows without WebEngine/Declarative (faster)
------------------------------------------------------

Use this for the `windows64-ubuntu24-noswap` path:

```shell
cd depends
make HOST=x86_64-w64-mingw32 QT_SKIP_WEBENGINE=1 QT_SKIP_QTDECLARATIVE=1 -j2
cd ..

./autogen.sh

QT_HOST_PREFIX="$PWD/depends/x86_64-w64-mingw32/native/qt-host"
QT_HOST_BIN="$QT_HOST_PREFIX/bin"
mkdir -p "$PWD/.qt-tool-aliases"
ln -sf "$QT_HOST_BIN/moc" "$PWD/.qt-tool-aliases/moc-qt6"
ln -sf "$QT_HOST_BIN/uic" "$PWD/.qt-tool-aliases/uic-qt6"
ln -sf "$QT_HOST_BIN/rcc" "$PWD/.qt-tool-aliases/rcc-qt6"

export PATH="$PWD/.qt-tool-aliases:$QT_HOST_BIN:$QT_HOST_PREFIX/libexec:$PWD/depends/x86_64-w64-mingw32/native/bin:$PATH"

CONFIG_SITE="$PWD/depends/x86_64-w64-mingw32/share/config.site" \
./configure --enable-scrypt-sse2 --prefix=/ --disable-bench --disable-tests \
  --with-gui=qt6 \
  --with-qt-bindir="$QT_HOST_BIN"

make -j1
```

32-bit Windows (i686-w64-mingw32)
----------------------------------

Install toolchain:

```shell
sudo apt-get install -y g++-mingw-w64-i686 mingw-w64-i686-dev
sudo update-alternatives --set i686-w64-mingw32-g++ /usr/bin/i686-w64-mingw32-g++-posix
```

Build:

```shell
cd depends
make HOST=i686-w64-mingw32 -j2
cd ..

./autogen.sh

QT_HOST_PREFIX="$PWD/depends/i686-w64-mingw32/native/qt-host"
QT_HOST_BIN="$QT_HOST_PREFIX/bin"
mkdir -p "$PWD/.qt-tool-aliases"
ln -sf "$QT_HOST_BIN/moc" "$PWD/.qt-tool-aliases/moc-qt6"
ln -sf "$QT_HOST_BIN/uic" "$PWD/.qt-tool-aliases/uic-qt6"
ln -sf "$QT_HOST_BIN/rcc" "$PWD/.qt-tool-aliases/rcc-qt6"

export PATH="$PWD/.qt-tool-aliases:$QT_HOST_BIN:$QT_HOST_PREFIX/libexec:$PWD/depends/i686-w64-mingw32/native/bin:$PATH"

CONFIG_SITE="$PWD/depends/i686-w64-mingw32/share/config.site" \
./configure --prefix=/ --disable-bench --disable-tests \
  --with-gui=qt6 \
  --with-qt-bindir="$QT_HOST_BIN"

make -j1
```

Optional strip:

```shell
cd src && i686-w64-mingw32-strip verged.exe verge-cli.exe verge-tx.exe
cd ../qt && i686-w64-mingw32-strip verge-qt.exe
```
