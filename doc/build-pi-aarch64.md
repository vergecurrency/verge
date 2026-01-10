## Building on Raspberry Pi 5

System:	64-bit
Kernel version:	6.12
Debian version:	13 (trixie)


Step 1. Install/Build all dependencies for aarch64 
```
apt install libseccomp-dev git build-essential xutils-dev libtool gperf autotools-dev automake pkg-config bsdmainutils libattr1-dev make automake bison byacc cmake curl bison byacc python3 libcap-dev
cd /verge/depends
make -j4 HOST=aarch64-linux-gnu
cd ..
```

Step 2. Compile Verge GUI Wallet/Full Node, Full Node Dameon, and CLI on Raspberry Pi 5! (To build Full Node Daemon and CLI -only-, skip this step and follow next step!)
```
./autogen.sh
CONFIG_SITE=$PWD/depends/aarch64-linux-gnu/share/config.site ./configure --build=aarch64-linux-gnu -disable-bench --disable-tests --disable-dependency-tracking --disable-werror --bindir=`pwd`/release/bin
make -j2
```

Optional Step 2. Compile Verge with Full Node Daemon and CLI only)
```
./autogen.sh
CONFIG_SITE=$PWD/depends/aarch64-linux-gnu/share/config.site ./configure --without-gui --build=aarch64-linux-gnu -disable-bench --disable-tests --disable-dependency-tracking --disable-werror --bindir=`pwd`/release/bin
make -j2
```