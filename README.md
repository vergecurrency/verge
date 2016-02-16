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

* Algorithm: scrypt
* PoW (proof of work)
* Blocktime: ~5 minutes
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

Approximately total reward: 9 Billion (9,000,000,000) during first year then issuing 1 billion (1,000,000,000) each year after.


Compiling Linux Wallet
----------------------

if you have never compiled a wallet in linux before, here are the dependencies you will need:

    sudo apt-get install build-essential pkg-config libtool autotools-dev autoconf automake libssl-dev libboost-all-dev  libminiupnpc-dev libdb++-dev libdb-dev qt4-qmake libqt4-dev libqrencode-dev libssl-dev git

to clone and compile:

    git clone https://github.com/vergecurrency/verge && cd verge/src && make -f makefile.unix

to make the qt gui wallet:

    git clone https://github.com/vergecurrency/verge && cd verge && qmake && make

then

type `sudo cp ~/verge/src/verged /usr/bin/` after you have typed that. Your Verge daemon will now be accessiable system wide.

after that has been done, type cd ~/ to get back to the home folder and type `verged` this will tell you that you need to make a VERGE.conf with it supplying you an output

that you can choose or not choose to use. So once you've ran that, then type `cd ~/.VERGE`, after that has been done type `sudo nano VERGE.conf`, paste the output from the `verged` command into the VERGE.conf like so
`rpcuser=bitcoinrpc
rpcpassword=85CpSuCNvDcYsdQU8w621mkQqJAimSQwCSJL5dPT9wQX - THESE ARE EXAMPLES`, once that has been completed proceed to add `rpcport=20102
port=21102 and daemon=1` below the rpcpassword. your config should look something like this
...
rpcuser=bitcoinrpc
rpcpassword=85CpSuCNvDcYsdQU8w621mkQqJAimSQwCSJL5dPT9wQX
rpcport=20102
port=21102
daemon=1
...


now just wait for the blockchain to download. you can check status by typing `./verged getinfo` in the ~/verge/src/ directory

Live Chat
---------

come check out our live chat:
[![Visit our IRC Chat!](https://kiwiirc.com/buttons/chat.freenode.net/verge.png)](https://kiwiirc.com/client/chat.freenode.net/?nick=xvg|?&theme=cli#verge)


