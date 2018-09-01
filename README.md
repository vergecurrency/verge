[![Build Status](https://travis-ci.org/vergecurrency/VERGE.svg?branch=master)](https://travis-ci.org/vergecurrency/VERGE)


```
____   _________________________   ________ ___________
\   \ /   /\_   _____/\______   \ /  _____/ \_   _____/
 \   Y   /  |    __)_  |       _//   \  ___  |    __)_
  \     /   |        \ |    |   \\    \_\  \ |        \ 2018 VERGE/XVG
   \___/   /_______  / |____|_  / \______  //_______  /
                   \/         \/         \/         \/
```

# VERGE [XVG] Source Code

## License

VERGE is released under the terms of the MIT license. See [LICENSE](LICENSE) for more
information or see https://opensource.org/licenses/MIT.

## Development Process

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/vergecurrency/VERGE/tags) are created
regularly to indicate new official, stable release versions of VERGE.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md).

## Testing

Testing and code review is the bottleneck for development; we get more pull requests than we can review and test on short notice. Please be patient and remember this is a security-critical project where any mistake might cost people lots of money.

## Specifications
Specification | Value
--- | ---
Protocol | PoW (proof of Work)
Algorithms | scrypt, x17, Lyra2rev2, myr-groestl, & blake2s
Blocktime | 30 seconds
Total Supply | 16,500,000,000 XVG
RPC port | 20102
P2P port | 21102
pre-mine | N/A
ICO | N/A

## Blockrewards
Block Number | Reward
--- | ---
0 to 14,000 | 200,000 coins
14,000 to 28,000 | 100,000 coins
28,000 to 42,000 | 50,000 coins
42,000 to 210,000 | 25,000 coins
210,000 to 378,000 | 12,500 coins
378,000 to 546,000 | 6,250 coins
546,000 to 714,000 | 3,125 coins
714,000 to 2,124,000 | 1,560 coins
2,124,000 to 4,248,000 | 730 coins

## Resources

* [Blockchain Explorer](https://verge-blockchain.info/)
* [Mining Pool List](https://vergecurrency.com/community/xvg-mining-pools/)
* [Black Paper](https://vergecurrency.com/static/blackpaper/Verge-Anonymity-Centric-CryptoCurrency.pdf)

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

## Building From Source

* [Linux Instructions](doc/build-verge-linux.md)
* [OS X Instructions](doc/build-verge-osx.md)
* [Windows Instructions](doc/build-verge-win.md)

## Docker Images

Check out the [`contrib/readme`](https://github.com/vergecurrency/VERGE/tree/master/contrib/docker) for more information.

### Windows Wallet

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

### Linux Wallet

1. Compile using [linux instructions](doc/build-verge-linux.md).

    If compilation was successful, the wallet GUI and the daemon binaries are `~/VERGE/src/qt/VERGE-qt` and `~/VERGE/src/VERGEd`respectively.

    After full installation, they are also in `/usr/local/bin/` directory.

    **Optional**: Copy binaries to your favourite location, e.g.:

    ```shell
    sudo cp ~/VERGE/src/VERGEd /path_to_VERGEd/VERGEd
    sudo cp ~/VERGE/src/qt/VERGE-qt /path_to_VERGE-qt/VERGE-qt
    ```
2. Start Verge daemon by running `VERGEd` or `/path_to_VERGEd/VERGEd`.

3. Redact ~/.VERGE/VERGE.conf, e.g.:

    ```shell
    nano ~/.VERGE/VERGE.conf
    ```

    Using this example, set your specific parameters (**Don't forget to change default rpcuser/rpcpassword**):

    ```
    rpcuser=vergerpcusername
    rpcpassword=85CpSuCNvDcYsdQU8w621mkQqJAimSQwCSJL5dPT9wQX
    rpcport=20102
    port=21102
    daemon=1
    algo=groestl
    ```

4. Start Verge daemon again.

> **Note:** To check the status of how much of the blockchain has been downloaded (aka synced) type `VERGEd getinfo` or `/path_to_VERGEd/VERGEd getinfo`.

> **Note**: If you see something like 'Killed (program cc1plus)' run ```dmesg``` to see the error(s)/problems(s). This is most likely caused by running out of resources. You may need to add some RAM or add some swap space.

You can also check out this [Linux Wallet Video Tutorial](https://www.youtube.com/watch?v=WYe75b6RWes).

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
ddvnucmtvyiemiuk.onion (sunerok)

## Donations

We believe in keeping Verge free and open. Any donations to help fuel the development effort are greatly appreciated! :smile:

* Address for donations in Verge (XVG): `DDd1pVWr8PPAw1z7DRwoUW6maWh5SsnCcp`
* Address for donations in Bitcoin (BTC): `142r3vCAH3AzABiQjFPmcrSCp6TDzEDuB1`

## Special Shout Outs

Special thanks to the following people that have helped make Verge possible. :raised_hands:

Sunerok, CryptoRekt, MKinney, BearSylla, Hypermist, Pallas1, FuzzBawls, BuZz, glodfinch, InfernoMan, AhmedBodi, BitSpill, MentalCollatz, ekryski and the **entire** #VERGE community!




# Bug Reporting

If you think you've found a bug or a problem with VERGE, please let us know! First, search our [issue](https://github.com/vergecurrency/VERGE/issues) tracker to see if someone has already reported the problem. If they haven't, click [here](https://github.com/vergecurrency/VERGE/issues/new) to open a new issue, and fill out the template with as much information as possible. The more you can tell us about the problem and how it occurred, the more likely we are to fix it.

## _Please do not report security vulnerabilities publicly._


## How to report a bug

### Code issues

Since we are a 100% open-source project we strongly prefer if you create a pull-request on Github in the proper repository with the necessary fix.

Alternatively, If you would like to make a suggestion regarding a potential fix please send an email to contact@vergecurrency.com


### Security-related issues

Contact the developers privately by sending an e-mail to contact@vergecurrency.com with the details of the issue. Do not post the issue on github or anywhere else until the issue has been resolved.

