[![Build Status](https://travis-ci.org/vergecurrency/VERGE.svg?branch=master)](https://travis-ci.org/vergecurrency/VERGE)
```
____   _________________________   ________ ___________
\   \ /   /\_   _____/\______   \ /  _____/ \_   _____/
 \   Y   /  |    __)_  |       _//   \  ___  |    __)_ 
  \     /   |        \ |    |   \\    \_\  \ |        \ 2016 VERGE/XVG
   \___/   /_______  / |____|_  / \______  //_______  /
                   \/         \/         \/         \/ 
```
VERGE [XVG] Source Code
================================

Specifications:
--------------

* Algorithms: scrypt, x17, Lyra2rev, myr-groestl, & blake2s
* PoW (proof of work)
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

Approximately total reward: 16.5 Billion


Compiling Linux Wallet
----------------------

if you have never compiled a wallet in linux before, here are the dependencies you will need:

    sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils git libboost-all-dev libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev
 
```
 then:
 
 sudo add-apt-repository ppa:bitcoin/bitcoin
 sudo apt-get update
 sudo apt-get install libdb4.8-dev libdb4.8++-dev
```

to clone and compile a daemon and gui wallet:

    git clone https://github.com/vergecurrency/verge && cd verge && ./autogen.sh && ./configure && make

then you now have the gui wallet in the /verge/src/qt and the daemon in /verge/src/

type `sudo cp ~/verge/src/VERGEd /usr/bin/` after you have typed that. Your Verge daemon will now be accessiable system wide. you can also do this with your gui wallet.

after that has been done, type cd ~/ to get back to the home folder and type `VERGEd` this will tell you that you need to make a VERGE.conf. So once you've ran that, then type `cd ~/.VERGE`, after that has been done type `sudo nano VERGE.conf`, paste the output from the `VERGEd` command into the VERGE.conf like so
```
rpcuser=vergerpcusername
rpcpassword=85CpSuCNvDcYsdQU8w621mkQqJAimSQwCSJL5dPT9wQX - THESE ARE EXAMPLES
```
Once that has been completed proceed to add `rpcport=20102 port=21102 and daemon=1` below the rpcpassword. your config should look something like this
```
rpcuser=vergerpcusername
rpcpassword=85CpSuCNvDcYsdQU8w621mkQqJAimSQwCSJL5dPT9wQX
rpcport=20102
port=21102
daemon=1
```
now exit the VERGE.conf by pressing ctrl + x on your keyboard then pressing `y` and hitting enter. this should have made your .conf save with all the stuff you just added,
if you wish you can check again by typing sudo nano VERGE.conf. After you've checked then exit the file the exact same way, then type `cd ~/` as before i said this takes you back to your home folder, you can now type verged and your verge daemon should boot.
To check the status of how much is synced type `verged getinfo`

Linux Wallet Video Tutorial
-------
https://www.youtube.com/watch?v=WYe75b6RWes

Live Chat
---------

come check out our live chat:
[![Visit our IRC Chat!](https://kiwiirc.com/buttons/chat.freenode.net/verge.png)](https://kiwiirc.com/client/chat.freenode.net/?nick=xvg|?&theme=cli#verge)


