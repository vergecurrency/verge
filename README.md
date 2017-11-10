[![Build Status](https://travis-ci.org/vergecurrency/VERGE.svg?branch=master)](https://travis-ci.org/vergecurrency/VERGE)


```
____   _________________________   ________ ___________
\   \ /   /\_   _____/\______   \ /  _____/ \_   _____/
 \   Y   /  |    __)_  |       _//   \  ___  |    __)_ 
  \     /   |        \ |    |   \\    \_\  \ |        \ 2017 VERGE/XVG
   \___/   /_______  / |____|_  / \______  //_______  /
                   \/         \/         \/         \/ 
```
VERGE [XVG] Source Code
================================

Specifications:
--------------

* PoW (proof of work)
* Algorithms: scrypt, x17, Lyra2rev2, myr-groestl, & blake2s
* Blocktime: 30 seconds
* RPC port: 20102
* P2P port: 21102
* Blockreward: 
  * Block 0 to 14,000 : 200,000 coins
  * 14,000 to 28,000 : 100,000 coins
  * 28,000 to 42,000: 50,000 coins
  * 42,000 to 210,000: 25,000 coins
  * 210,000 to 378,000: 12,500 coins
  * 378,000 to 546,000: 6,250 coins
  * 546,000 to 714,000: 3,125 coins
  * 714,000 to 2,124,000: 1,560 coins
  * 2,124,000 to 4,248,000: 730 coins

Total Supply
------------

Approximately total reward cap: 16.5 Billion

Binary (pre-compiled) wallets are available on all platforms at [https://vergecurrency.com](https://vergecurrency.com/#wallets)

Compiling Linux Wallet on Ubuntu/Debian (faster) 
----------------------

sudo rm -Rf ~/VERGE  #(if you already have it) 

sudo apt-get -y install git && cd ~ && git clone https://github.com/vergecurrency/VERGE && cd VERGE && sh go.sh


Compiling Linux Wallet on Ubuntu/Debian
----------------------

Step 1. Install the dependencies. 

```sudo add-apt-repository ppa:bitcoin/bitcoin```

```sudo apt-get update```

```sudo apt-get install libdb4.8-dev libdb4.8++-dev build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils git libboost-all-dev libminiupnpc-dev libqt5gui5 libqt5core5a libqt5webkit5-dev libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev```

**Note**: If you are on debian, you will also need to `apt-get install libcanberra-gtk-module`.

Step 2. Clone the git repository and compile the daemon and gui wallet:

```git clone https://github.com/vergecurrency/verge && cd verge && ./autogen.sh && ./configure && make```

**Note**: If you get a "memory exhausted" error, make a swap file. (https://www.digitalocean.com/community/tutorials/how-to-add-swap-space-on-ubuntu-16-04)

Using the wallet:
----
The gui wallet is in ./verge/src/qt and the daemon in ./verge/src directories.

**Note**: If you see something like 'Killed (program cc1plus)' run ```dmesg``` to see the error(s)/problems(s). This is most likely caused by running out of resources. You may need to add some RAM or add some swap space.

**Optional**:
If you want to copy the binaries for use by all users, run the following commands:

```sudo cp src/VERGEd /usr/bin/```

```sudo cp src/qt/VERGE-qt /usr/bin/```

Step 3. Creating a configuration file. Type ```cd ~``` to get back to the home folder and type:

```VERGEd.exe```  (or ```./VERGEd``` if on mac or linux)

the output from this command will tell you that you need to make a VERGE.conf and will suggest some good starting values.

For Linux users, type:
 
```nano ~/.VERGE/VERGE.conf```
(For Windows users, see below. For mac users, the command is ```nano ~/Library/Application\ Support\VERGE\VERGE.conf```)
    
Paste the output from the `VERGEd` command into the VERGE.conf like this: (It is recommended to change the password to something unique.)

    rpcuser=vergerpcusername
    rpcpassword=85CpSuCNvDcYsdQU8w621mkQqJAimSQwCSJL5dPT9wQX
    
    
**Optional**: Add `rpcport=20102`, `port=21102`, or `algo=groestl` to the configuration file.

Add `daemon=1`. 

Your config may look something like this:

    rpcuser=vergerpcusername
    rpcpassword=85CpSuCNvDcYsdQU8w621mkQqJAimSQwCSJL5dPT9wQX
    rpcport=20102
    port=21102
    daemon=1
    algo=groestl

Exit the VERGE.conf by pressing `ctrl + x` on your keyboard then pressing `y` and hitting enter. This should have created a VERGE.conf file with what you just added. 

Type ```VERGEd.exe``` (or ```./VERGEd``` if on mac or linux) and your verge daemon should start.

To check the status of how much of the blockchain has been downloaded (aka synced) type `VERGEd.exe getinfo` (or `./VERGEd getinfo` if on mac or linux).



To compile on Mac (OSX El Capitan, but test compiled on Mountain Lion v10.8):
------------
1. Ensure you do not have qt5 nor qt installed.

    `brew uninstall qt qt5 qt55 qt52`   
   
2. Download [qt5.4.2](https://download.qt.io/official_releases/qt/5.4/5.4.2/qt-opensource-mac-x64-clang-5.4.2.dmg)
     
3. Install qt5 into /usr/local/qt5
   
   Note: Change the installation folder from "xxx/Qt5.4.2" to "/usr/local/qt5"
   
4. Run these commands:

    `export PKG_CONFIG_PATH=/usr/local/qt5/5.4/clang_64/lib/pkgconfig`
    
    `export PATH=/usr/local/qt5/5.4/clang_64/bin:$PATH`
    
    `export QT_CFLAGS="-I/usr/local/qt5/5.4/clang_64/lib/QtWebKitWidgets.framework/Versions/5/Headers -I/usr/local/qt5/5.4/clang_64/lib/QtWebView.framework/Versions/5/Headers -I/usr/local/qt5/5.4/clang_64/lib/QtDBus.framework/Versions/5/Headers -I/usr/local/qt5/5.4/clang_64/lib/QtWidgets.framework/Versions/5/Headers -I/usr/local/qt5/5.4/clang_64/lib/QtWebKit.framework/Versions/5/Headers -I/usr/local/qt5/5.4/clang_64/lib/QtNetwork.framework/Versions/5/Headers -I/usr/local/qt5/5.4/clang_64/lib/QtGui.framework/Versions/5/Headers -I/usr/local/qt5/5.4/clang_64/lib/QtCore.framework/Versions/5/Headers -I. -I/usr/local/qt5/5.4/clang_64/mkspecs/macx-clang -F/usr/local/qt5/5.4/clang_64/lib"`
    
    `export QT_LIBS="-F/usr/local/qt5/5.4/clang_64/lib -framework QtWidgets -framework QtGui -framework QtCore -framework DiskArbitration -framework IOKit -framework OpenGL -framework AGL -framework QtNetwork -framework QtWebKit -framework QtWebKitWidgets -framework QtDBus -framework QtWebView"`
    
    
5. Install the other required items:

    `brew install protobuf boost miniupnpc openssl qrencode berkeley-db4 automake`
    
6. Download the wallet source and build:

    `git clone https://github.com/vergecurrency/verge`

    `cd verge`
    
    `./autogen.sh`
    
    `./configure --with-gui=qt5`
    
    `make -j4`
    

Building the Mac installer (.dmg) file
-----
Run `make deploy`

If you are building the .dmg (by running 'mac deploy') you may need to run these commands if you get an error regarding mysql:

    brew install mysql
    
    cd /usr/local/qt5/5.4/clang_64/plugins/sqldrivers
    
    otool -L libqsqlmysql.dylib

Note: This may be pointing to an version of mysql that you do not have installed (like mysql55) - Alternatively, you may be able to remove the library from the sqldrivers folder.

    install_name_tool -change /opt/local/lib/mysql55/mysql/libmysqlclient.18.dylib /usr/local/Cellar/mysql/5.7.12/lib/libmysqlclient.20.dylib libqsqlmysql.dylib
    
Trying to build .dmg on 10.8? You will need to run this:
    
    export CFLAGS=-Qunused-arguments
    
    export CPPFLAGS=-Qunused-arguments
    
    sudo -E easy_install appscript
    

Want to use Docker?
------------

Check out the [readme](https://github.com/vergecurrency/VERGE/tree/master/contrib/docker) for more information. [Official VERGE docker images](https://hub.docker.com/r/vergecurrency/verge/)

Want to to download the blockchain instead of waiting/syncing? (quicker)
------------
download this .zip file, and extract it to your verge data directory:
https://verge-blockchain.com (follow the easy HOW TO instructions!)
http://108.61.216.160/cryptochainer.chains/chains/Verge_blockchain.zip

Want to 'solo-mine' from the wallet?
----------
You can use the wallet to mine. You need to specify the algorithm (see below) and set the "gen" flag. For instance, in the configuration specify ```gen=1```. 


Using different algorithms (for mining)
----------

To use a specific mining algorithm use the `algo` switch in your configuration file (.conf file) or from the command line (like this `--algo=x17`) Here are the possible values:

    algo=x17
    algo=scrypt
    algo=groestl
    algo=lyra
    algo=blake

Using VERGE on Windows
-------------

1. Download the pre-compiled software. (only from official VERGE site)
2. Install
3. In windows file explorer, open c:\Users\XXX\AppData\Roaming\VERGE (be sure to change XXX to your windows user)
4. Right click and create a new file verge.txt
5. Edit the file to have contents above (see instructions above for options)
6. Save and close the file
7. Reame the file to verge.conf
8. Start the VERGE-qt program.
9. Open up VERGE-qt console and run ```getinfo``` (or ```getmininginfo```) to verify settings.

**Note:** You must re-start the wallet after making changes to verge.conf.


Thanks
----
Special Thanks to: Sunerok, CryptoRekt, MKinney, BearSylla, Hypermist, Pallas1, FuzzBawls, BuZz, glodfinch, InfernoMan, AhmedBodi, BitSpill, MentalCollatz, and the -entire- #VERGE irc community!


Linux Wallet Video Tutorial
-------
https://www.youtube.com/watch?v=WYe75b6RWes

Live Chat
---------

Come check out our live chat:

[![Visit our IRC Chat!](https://kiwiirc.com/buttons/chat.freenode.net/verge.png)](https://kiwiirc.com/client/chat.freenode.net/?nick=xvg|?&theme=cli#verge)


