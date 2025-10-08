<p align="center"><img src="https://raw.githubusercontent.com/vergecurrency/VERGE/master/readme-header.png" alt="Verge Source Code"></p>
<p align="center">
  <img src="https://img.shields.io/badge/status-stable-green.svg">
  <img src="https://img.shields.io/badge/license-MIT-blue.svg">
  <img src="https://img.shields.io/badge/C%2B%2B-17%2F20-blue.svg">
  <a href="https://github.com/vergecurrency/verge/releases/latest"><img alt="GitHub all releases" src="https://img.shields.io/github/downloads/vergecurrency/verge/total?logo=GitHub"></a>
  <img alt="GitHub commit activity" src="https://img.shields.io/github/commit-activity/m/vergecurrency/VERGE">
  <img alt="GitHub Repo stars" src="https://img.shields.io/github/stars/vergecurrency/verge">
  <img alt="Discord" src="https://img.shields.io/discord/325024453065179137">
  <img alt="GitHub language count" src="https://img.shields.io/github/languages/count/vergecurrency/verge">
 <img alt="X (formerly Twitter) Follow VergeCurrency!" src="https://img.shields.io/twitter/follow/vergecurrency?logo=twitter&logoColor=teal&labelColor=black&color=black">
</p>
<br>

# VERGE Source Code [XVG]

Latest commit is stable, building on all platforms?:
<p align="left">
  <a href="https://github.com/vergecurrency/verge/actions/workflows/check-commit.yml">
  <img src="https://github.com/vergecurrency/verge/actions/workflows/check-commit.yml/badge.svg">
  </a>
</p>

## Build Requirements

Verge Core requires a modern C++ toolchain and has migrated to reduce external dependencies for better performance and maintainability.

### Minimum Requirements

| Component | Requirement | Notes |
|-----------|-------------|--------|
| **Compiler** | GCC 9.0+ / Clang 10.0+ / MSVC 2019+ | C++17 support required |
| **C++ Standard** | C++17 (targeting C++20) | Modern features for better performance |
| **CMake** | 3.16+ | Recommended build system |
| **Boost** | 1.70+ (selective components) | Reduced dependency footprint |

### Supported Operating Systems

| OS | Version | Architecture |
|----|---------|--------------|
| **Ubuntu** | 22.04, 24.04 | x64, ARM64 |
| **Debian** | 11+ | x64, ARM64 |
| **CentOS/RHEL** | 8+ | x64 |
| **macOS** | 13.0+ (Ventura), 14.0 (Sonoma) |x64, ARM64 (Apple Silicon) |
| **Windows** | 10 32bit/64bit, 11 32bit/64bit | x64

## Modern C++ Migration Benefits

### 🚀 **Performance Improvements**
- **Faster Compilation**: Reduced template instantiation overhead
- **Better Optimization**: Modern compiler optimizations with C++17/20
- **Memory Efficiency**: Smart pointers and RAII reduce memory leaks
- **Parallel Processing**: Standard library threading primitives

### 📦 **Reduced Dependencies**
- **Smaller Binary Size**: Less dependency on external libraries
- **Easier Deployment**: Fewer runtime dependencies to manage
- **Simplified Building**: Standard library features reduce complex linking

### 🛡️ **Enhanced Security & Reliability**
- **Memory Safety**: Smart pointers prevent common vulnerabilities
- **Type Safety**: Modern C++ type system catches errors at compile time
- **Thread Safety**: Better concurrency primitives prevent race conditions
- **Exception Safety**: RAII patterns ensure proper resource cleanup

### 🔧 **Developer Experience**
- **Modern Tooling**: Better IDE support and debugging
- **Cleaner Code**: More expressive and maintainable codebase
- **Faster Development**: Standard library features reduce boilerplate
- **Better Testing**: Modern testing frameworks and practices

## Specifications

Specification | Value
--- | ---
Protocol | PoW (proof of Work)
Algorithms | scrypt, x17, Lyra2rev2, myr-groestl, & blake2s
Blocktime | 30 seconds
Total Supply | 16,521,951,238 XVG (Complete!)
RPC port | 20102 (testnet: 21102)
P2P port | 21102 (testnet: 21104)
pre-mine | Not Applicable
ICO | Not Applicable

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
7,200,001+             | 0.0 coins

## Resources

* [Blockchain Explorer] (https://verge-blockchain.info/) https://verge-blockchain.info/
* [Blockchain Explorer] (https://xvgblockexplorer.com/) https://xvgblockexplorer.com/
* [Blockchain Explorer Testnet] (https://testnet.verge-blockchain.info/) https://testnet.verge-blockchain.info/
* [Network Hash and Difficulty] (https://vergecurrency.network/d/VmzuEE5Mk/verge-blockchain?) https://vergecurrency.network/d/VmzuEE5Mk/verge-blockchain
* [Network Transaction Information] (https://network.verge-blockchain.com/d/e8a7802b-23c3-4d81-88f3-fcd0e2efb235/) https://network.verge-blockchain.com/d/e8a7802b-23c3-4d81-88f3-fcd0e2efb235/ (all real transactions only! empty blocks, mining pool txs, etc are filtered out!)
* [Mining Pool List] (https://miningpoolstats.stream/) Most pools of our 5 algorithms can be found here!
* [Black Paper] (https://vergecurrency.com/static/blackpaper/verge-blackpaper-v5.0.pdf) Verge's whitepaper can be found here!

### Community

* [Telegram](https://t.me/officialxvg)
* [Discord](https://discord.gg/vergecurrency)
* [Twitter](https://www.twitter.com/vergecurrency)
* [Facebook](https://www.facebook.com/VERGEcurrency/)
* [Reddit](https://www.reddit.com/r/vergecurrency/)

## Wallets

Binary (pre-compiled) wallets are available on all platforms here in [Releases](https://github.com/vergecurrency/verge/releases).

> **Note:** **Important!** Only download pre-compiled wallets from the official Verge website or official Github repos.

> **Note:** For a fresh wallet install you can reduce the blockchain syncing time by downloading [a nightly snapshot](https://verge-blockchain.com/down) and following the [setup instructions](https://verge-blockchain.com/howto).

### Windows Wallet Usage

1. Download the pre-compiled software from the releases section here.
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

### MacOS Wallet

1. Download the pre-compiled macos software from the releases section here.
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

### Modern C++ Migration

Verge Core is actively migrating to modern C++ standards and practices:

- **C++17 Standard**: Full adoption with selective C++20 features
- **Reduced Boost Dependencies**: Migrating to standard library equivalents
- **Smart Pointers**: RAII and memory safety improvements
- **Thread Safety**: Modern concurrency patterns with `std::mutex` and `std::atomic`

### Coding Standards

- Use `std::filesystem` instead of `boost::filesystem`
- Prefer `std::thread` over `boost::thread`
- Use `std::unique_ptr`/`std::shared_ptr` instead of raw pointers
- Apply `const` correctness and `noexcept` specifications
- Follow RAII principles for resource management

### Quick Build (Ubuntu/Debian)

```shell
# Install modern dependencies
sudo apt update
sudo apt install -y \
    build-essential cmake git \
    libssl-dev libevent-dev \
    libdb4.8-dev libdb4.8++-dev \
    libboost-system-dev libboost-filesystem-dev libboost-test-dev \
    libminiupnpc-dev libqt5gui5 libqt5core5a libqt5dbus5 \
    qttools5-dev qttools5-dev-tools \
    libprotobuf-dev protobuf-compiler \
    libqrencode-dev libseccomp-dev libcap-dev

# Clone and build
git clone https://github.com/vergecurrency/VERGE && cd VERGE
./autogen.sh && ./configure --enable-cxx17 && make -j$(nproc)
```

### Advanced Build with CMake (Recommended)

```shell
# Modern build approach
mkdir build && cd build
cmake .. -DCMAKE_CXX_STANDARD=17 -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel $(nproc)
```

> **Performance Tip**: Use `make -j$(nproc)` or `cmake --build . --parallel` to utilize all CPU cores.

> **Memory Note**: If you encounter memory issues, either add swap space or reduce parallelism with `-j2`.




### macOS Development

> **Requirements:** macOS 12.0+ (Monterey), Xcode 13.0+, Command Line Tools

#### Intel and Apple Silicon Support

```shell
# Install Homebrew dependencies
brew install cmake boost openssl libevent berkeley-db4
brew install qt5 protobuf qrencode miniupnpc

# For Apple Silicon Macs, you may need to specify paths
export PATH="/opt/homebrew/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"

# Clone and build
git clone https://github.com/vergecurrency/VERGE && cd VERGE
./autogen.sh
./configure --enable-cxx17 --with-boost=/opt/homebrew
make -j$(sysctl -n hw.ncpu)
```

#### CMake Build (Recommended for macOS)

```shell
mkdir build && cd build
cmake .. \
    -DCMAKE_CXX_STANDARD=17 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="/opt/homebrew/opt/qt5" \
    -DOPENSSL_ROOT_DIR="/opt/homebrew/opt/openssl"
cmake --build . --parallel $(sysctl -n hw.ncpu)
```

> **Apple Silicon Note**: Some dependencies may need explicit paths. Use `brew --prefix` to find installation directories.

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

