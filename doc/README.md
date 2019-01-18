Verge Core
=============

Setup
---------------------
Verge Core is the original Verge client and it builds the backbone of the network. It downloads and, by default, stores the entire history of Verge transactions (which is currently more than 2 GBs); depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more.

To download Verge Core, visit [Verge's Github](https://github.com/vergecurrency/VERGE/releases).

Running
---------------------
The following are some helpful notes on how to run Verge on your native platform.

### Unix

Unpack the files into a directory and run:

- `bin/verge-qt` (GUI) or
- `bin/verged` (headless)

### Windows

Unpack the files into a directory, and then run verge-qt.exe.

### macOS

Drag verge-Core to your applications folder, and then run verge-Core.

### Need Help?

* See the documentation at the [Verge Wiki](https://verge.zendesk.com)
for help and more information.
* Ask for help on [#vergecurrency](https://discord.gg/vergecurrency) on Discord.

Building
---------------------
The following are developer notes on how to build Verge on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [macOS Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [NetBSD Build Notes](build-netbsd.md)
- [Gitian Building Guide](gitian-building.md)

Development
---------------------
The Verge repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://dev.visucore.com/verge/doxygen/)
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [Travis CI](travis-ci.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)

### Resources
* Discuss general development on #development on Discord [here](https://discord.gg/vergecurrency).

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [ZMQ](zmq.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.
