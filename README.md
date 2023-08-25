<p align="center"><img src="https://raw.githubusercontent.com/vergecurrency/VERGE/master/readme-header.png" alt="Verge Source Code"></p>
<p align="center">
  <a href="https://circleci.com/gh/vergecurrency/verge/tree/master" target="_blank"><img src="https://circleci.com/gh/vergecurrency/verge/tree/master.svg?style=svg"></a>
  <img src="https://img.shields.io/badge/status-stable-green.svg">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg">
  <a href="https://codecov.io/gh/vergecurrency/VERGE">
    <img src="https://codecov.io/gh/vergecurrency/VERGE/branch/master/graph/badge.svg" />
  </a>
  <a href="https://github.com/vergecurrency/VERGE/releases">
    <img alt="GitHub All Releases" src="https://img.shields.io/github/downloads/vergecurrency/VERGE/total?style=social">
  </a>
  <img alt="GitHub commit activity" src="https://img.shields.io/github/commit-activity/m/vergecurrency/VERGE">
</p>

# VERGE Source Code [XVG]

## Specifications
Specification | Value
--- | ---
Protocol | PoW (proof of Work)
Algorithms | scrypt, x17, Lyra2rev2, myr-groestl, & blake2s
Blocktime | 30 seconds
Total Supply | 16,500,000,000 XVG
RPC port | 20102 (testnet: 21102)
P2P port | 21102 (testnet: 21104)
pre-mine | N/A
ICO | N/A

## Blockrewards
Block Number Range | Reward
--- | ---
0 to 14,000 | 200,000 coins
14,001 to 28,000 | 100,000 coins
28,001 to 42,000 | 50,000 coins
42,001 to 210,000 | 25,000 coins
210,001 to 378,000 | 12,500 coins
378,001 to 546,000 | 6,250 coins
546,001 to 714,000 | 3,125 coins
714,001 to 2,124,000 | 1,560 coins
2,124,001 to 3,700,000 | 730 coins
3,700,001 to 4,200,000 | 400 coins
4,200,001 to 4,700,000 | 200 coins
4,700,001 to 5,200,000 | 100 coins
5,200,001 to 5,700,000 | 50  coins
5,700,001 to 6,200,000 | 25  coins
6,200,001 to 6,700,000 | 12.5 coins
6,700,001 to 7,200,000 | 6.25 coins

## Resources

* [Blockchain Explorer](https://verge-blockchain.info/)
* [Blockchain Explorer Testnet](https://testnet.verge-blockchain.info/)
* [Mining Pool List](https://vergecurrency.com/community/xvg-mining-pools/)
* [Black Paper](https://vergecurrency.com/static/blackpaper/verge-blackpaper-v5.0.pdf)

### Community

* [Telegram](https://t.me/VERGExvg)
* [Discord](https://discord.gg/vergecurrency)
* [Twitter](https://www.twitter.com/vergecurrency)
* [Facebook](https://www.facebook.com/VERGEcurrency/)
* [Reddit](https://www.reddit.com/r/vergecurrency/)

## Wallets

Binary (pre-compiled) wallets are available on all platforms at [https://vergecurrency.com](https://vergecurrency.com/#wallets).

> **Note:** **Important!** Only download pre-compiled wallets from the official Verge website or official Github repos.

> **Note:** For a fresh wallet install you can reduce the blockchain syncing time by downloading [a nightly snapshot](https://verge-blockchain.com/down) and following the [setup instructions](https://verge-blockchain.com/howto).

### Windows Wallet Usage

1. Download the pre-compiled software.
2. Install
3. In windows file explorer, open `c:\Users\XXX\AppData\Roaming\VERGE` (be sure to change XXX to your windows user)
4. Right click and create a new file `verge.txt`
5. Edit the file to have the following contents (be sure to change the password)

    ```
    rpcuser=vergerpcusername
    rpcpassword=85CpSuCNvDcYsdQU8w621mkQqJAimSQwCSJL5dPT9wQX
    rpcport=20102
    port=21102
    daemon=1
    algo=groestl
    ```

6. Save and close the file
7. Rename the file to `verge.conf`
8. Start the VERGE-qt program.
9. Open up VERGE-qt console and run `getinfo` (or `getmininginfo`) to verify settings.

> **Note:** You must re-start the wallet after making changes to `verge.conf`.

### OS X Wallet

1. Download the pre-compiled software.
2. Double click the DMG
3. Drag the Verge-Qt to your Applications folder
4. Double click the Verge-Qt application to open it.
5. Go grab a :coffee: while it syncs with the blockchain

> **Note:** It may look like it is frozen or hung while it is indexing and syncing the blockchain. It's not. It's chugging away, but currently the UI doesn't give you a lot of feedback on status. We're working to fix that. Syncing takes a while to complete (ie. > 10 minutes or more) so just be patient.

> **Note:** If you want to change your configuration the file is located at `~/Library/Application\ Support\VERGE\VERGE.conf`. This isn't required by default.

### Unix Wallet

1. Compile using [Unix instructions](doc/build-unix.md).
2. The wallet GUI is in `./verge/src/qt` and the daemon in `./verge/src`.
3. **Optional** - the binaries to your favorite location. for use by all users, run the following commands:

    ```shell
    sudo cp src/VERGEd /usr/bin/
    sudo cp src/qt/VERGE-qt /usr/bin/
    ```

4. Run `./VERGEd` from wherever you put it. The output from this command will tell you that you need to make a `VERGE.conf` file and will suggest some good starting values.
5.  Open up your new config file that was created in your home directory in your favorite text editor

    ```shell
    nano ~/.VERGE/VERGE.conf
    ```

6. Paste the output from the `VERGEd` command into the VERGE.conf like this: (It is recommended to change the password to something unique.)

    ```
    rpcuser=vergerpcusername
    rpcpassword=85CpSuCNvDcYsdQU8w621mkQqJAimSQwCSJL5dPT9wQX
    rpcport=20102
    port=21102
    daemon=1
    algo=groestl
    ```

7. Save the file and exit your editor. If using `nano` type `ctrl + x` on your keyboard and the `y` and hitting enter. This should have created a `VERGE.conf` file with what you just added.

8. Start the Verge daemon again

    ```shell
    ./path/to/VERGEd
    ```

> **Note:** To check the status of how much of the blockchain has been downloaded (aka synced) type `./path/to/VERGEd getinfo`.

> **Note**: If you see something like 'Killed (program cc1plus)' run ```dmesg``` to see the error(s)/problems(s). This is most likely caused by running out of resources. You may need to add some RAM or add some swap space.

You can also check out this [Linux Wallet Video Tutorial](https://www.youtube.com/watch?v=WYe75b6RWes).

## Building From Source

* [Unix Instructions](doc/build-unix.md)
* [OS X Instructions](doc/build-osx.md)
* [Windows Instructions](doc/build-windows.md)


## Use [Unstoppable Domains](https://unstoppabledomains.com)

To use VERGE with Unstoppable Domains for sending coins using Web3 Domains (know more about [Unstoppable Domains here](https://unstoppabledomains.com)), use this command to start the app "./verge-qt --with-unstoppable".

## Developer Notes

The Easy Method:

> **Note**: Sometimes linux user permissions are not set up properly, and causes failed compiling in linux. Please ensure your user has access or do the install from root if these problems arise.

```shell
sudo rm -Rf ~/VERGE  #(if you already have it)
sudo apt-get -y install git && cd ~ && git clone https://github.com/vergecurrency/VERGE && cd VERGE && sh go.sh
```

The _slightly_ longer version:

1. Install the dependencies. **Note**: If you are on debian, you will also need to `apt-get install libcanberra-gtk-module`.

    ```shell
    sudo add-apt-repository ppa:bitcoin/bitcoin
    sudo apt-get update
    sudo apt-get install \
        libdb4.8-dev libdb4.8++-dev build-essential \
        libtool autotools-dev automake pkg-config libssl-dev libevent-dev \
        bsdmainutils git libboost-all-dev libminiupnpc-dev libqt5gui5 \
        libqt5core5a libqt5dbus5 libevent-dev qttools5-dev \
        qttools5-dev-tools libprotobuf-dev protobuf-compiler libqrencode-dev \
        libseccomp-dev libcap-dev
    ```

2. Clone the git repository and compile the daemon and gui wallet:

    ```shell
    git clone https://github.com/vergecurrency/VERGE && cd VERGE && ./autogen.sh && ./configure && make
    ```
    If updating from previous version, dont forget to:
    ```shell
    sudo make install
    ```

> **Note**: If you get a "memory exhausted" error, make a swap file. (https://www.digitalocean.com/community/tutorials/how-to-add-swap-space-on-ubuntu-16-04)




### Mac OS X Wallet

> **Note:** This has only been confirmed to work on OS X Sierra (10.12) and OS X High Sierra (10.13) with XCode 9.2 and `Apple LLVM version 9.0.0 (clang-900.0.39.2)`.

1. Ensure you have mysql and boost installed.
    
    ```shell
    brew install mysql boost
    ```

2. Ensure you have python 2.7 installed and in your path (OS X comes with this by default)

    ```shell
    python --version
    ```

3. Export the required environment variables

    ```shell
    export VERGE_PLATFORM='mac'
    export CXX=clang++
    export CC=clang
    ```

4. Run your build commands

    ```shell
    ./building/common.sh
    ./building/mac/requirements.sh
    ./building/mac/build.sh
    ```

5. Grab a :coffee: and wait it out

6. Create the `.dmg` file

    ```shell
    ./building/mac/dist.sh
    ```

### Windows Wallet

TODO. Take a look at [building/windows](./building/windows).

## Docker Images

Check out the [`contrib/readme`](https://github.com/vergecurrency/VERGE/tree/master/contrib/docker) for more information.

## Mining

### Solo mining

Instead of joining a mining pool you can use the wallet to mine all by yourself. You need to specify the algorithm (see below) and set the "gen" flag. For instance, in the configuration specify `gen=1`.

### Using different algorithms

To use a specific mining algorithm use the `algo` switch in your configuration file (`.conf` file) or from the command line (like this `--algo=x17`). Here are the possible values:

```
algo=x17
algo=scrypt
algo=groestl
algo=lyra
algo=blake
```

## TestNet

Here is a list of active testnet nodes:
* ddvnucmtvyiemiuk.onion (sunerok)
* uacxdw34wnfybshfjs6hxdfzwkqxs765peu4iyyakqnz2mqyvspubeqd.onion (dedicated testnet)

## Donations

We believe in keeping Verge free and open. Any donations to help fuel the development effort are greatly appreciated! :smile:

* Address for donations in Verge (XVG): `DDd1pVWr8PPAw1z7DRwoUW6maWh5SsnCcp`
* Address for donations in Bitcoin (BTC): `142r3vCAH3AzABiQjFPmcrSCp6TDzEDuB1`

## Special Shout Outs

Special thanks to the following people that have helped make Verge possible. :raised_hands:

Sunerok, CryptoRekt, MKinney, BearSylla, Hypermist, Pallas1, FuzzBawls, BuZz, glodfinch, InfernoMan, AhmedBodi, BitSpill, MentalCollatz, ekryski and the **entire** #VERGE community!




# Bug Reporting

If you think you've found a bug or a problem with VERGE, please let us know! First, search our issue tracker to see if someone has already reported the problem. If they haven't, open a new issue, and fill out the template with as much information as possible. The more you can tell us about the problem and how it occurred, the more likely we are to fix it.

## _Please do not report security vulnerabilities publicly._


## How to report a bug

### Code issues

Since we are a 100% open-source project we strongly prefer if you create a pull-request on Github in the proper repository with the necessary fix.

Alternatively, if you would like to make a suggestion regarding a potential fix please send an email to contact@vergecurrency.com


### Security-related issues

Contact the developers privately by sending an e-mail to contact@vergecurrency.com with the details of the issue. Do not post the issue on github or anywhere else until the issue has been resolved.

