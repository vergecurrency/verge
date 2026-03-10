Linux Build Instructions (Ubuntu 24.04)
=======================================

This document matches the Linux build path used in `.github/workflows/check-commit.yml`.

Prerequisites
-------------

Install required packages:

```shell
sudo apt-get update
sudo apt-get install -y \
  build-essential xutils-dev libtool gperf autotools-dev automake pkg-config \
  bsdmainutils libattr1-dev make automake bison byacc cmake curl \
  g++-multilib binutils-gold python3 python3-setuptools gyp ccache ninja-build flex nodejs \
  libgl1-mesa-dev libegl1-mesa-dev libopengl-dev
```

Build (default Qt build)
------------------------

```shell
cd depends
make -j4 HOST=x86_64-linux-gnu
cd ..

./autogen.sh

CONFIG_SITE="$PWD/depends/x86_64-linux-gnu/share/config.site" \
./configure --enable-scrypt-sse2 --disable-bench --disable-tests \
  --disable-dependency-tracking --disable-werror \
  --prefix="$PWD/depends/x86_64-linux-gnu" \
  --bindir="$PWD/release/bin" \
  --libdir="$PWD/release/lib"

make -j4
```

Optional strip:

```shell
cd src && strip verged verge-cli verge-tx
cd ../qt && strip verge-qt
```

Build Without Qt WebEngine (faster)
-----------------------------------

This is the no-WebEngine / no-QtDeclarative path used by `ubuntu24-noswap`.

```shell
cd depends
make -j4 HOST=x86_64-linux-gnu QT_SKIP_WEBENGINE=1 QT_SKIP_QTDECLARATIVE=1
cd ..

./autogen.sh

CONFIG_SITE="$PWD/depends/x86_64-linux-gnu/share/config.site" \
./configure --enable-scrypt-sse2 --disable-bench --disable-tests \
  --disable-dependency-tracking --disable-werror \
  --prefix="$PWD/depends/x86_64-linux-gnu" \
  --bindir="$PWD/release/bin" \
  --libdir="$PWD/release/lib"

make -j4
```
