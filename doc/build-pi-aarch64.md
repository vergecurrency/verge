Raspberry Pi / AArch64 Build Instructions
=========================================

This document matches the Raspberry Pi job in `.github/workflows/check-commit.yml`.

Prerequisites (Ubuntu 24.04 ARM / Debian ARM64)
------------------------------------------------

```shell
sudo apt-get update
sudo apt-get install -y \
  libseccomp-dev git build-essential xutils-dev libtool gperf autotools-dev \
  automake pkg-config bsdmainutils libattr1-dev make automake bison byacc \
  cmake curl bison byacc python3 libcap-dev gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
  ninja-build flex nodejs gyp
```

Build (Qt without WebEngine)
----------------------------

```shell
cd depends
make -j1 HOST=aarch64-linux-gnu QT_SKIP_WEBENGINE=1
cd ..

./autogen.sh

CONFIG_SITE="$PWD/depends/aarch64-linux-gnu/share/config.site" \
LIBCAP_LIBS="-lcap" \
./configure \
  --host=aarch64-linux-gnu \
  --build=aarch64-linux-gnu \
  --disable-bench \
  --disable-tests \
  --disable-dependency-tracking \
  --disable-werror \
  --prefix="$PWD/depends/aarch64-linux-gnu" \
  --bindir="$PWD/release/bin" \
  --libdir="$PWD/release/lib"

make -j4
```

Optional strip:

```shell
cd src && strip verged verge-cli verge-tx
cd ../qt && strip verge-qt
```
