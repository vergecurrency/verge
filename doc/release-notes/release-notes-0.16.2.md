---
# This file is licensed under the MIT License (MIT) available on
# http://opensource.org/licenses/MIT.
# Text originally from Bitcoin Core project
# Metadata and small formatting changes from Bitcoin.org project

## Required value below populates the %v variable (note: % needs to be escaped in YAML if it starts a value)
required_version: 0.16.2
## Required title.
title: Bitcoin Core version 0.16.2 released
## Optional release date.  May be filled in hours/days after a release
optional_date: 2018-07-29

---

<div class="post-content" markdown="1">

Important Notice
==============

The information contained in this document originated from the Bitcoin Core project. 

This document is to serve as a reference to the changes that where implemented during the most recent VERGE code base migration. 

---

Please report bugs using the issue tracker at github:

  <https://github.com/vergecurrency/VERGE/issues>

How to Upgrade
==============

If you are running an older version, shut it down. Wait until it has completely
shut down (which might take a few minutes for older versions), then create a directory in the same root folder as you setup previously, and unzip. 
It is good practice to call the folder you create the version number. 
On Mac, copy over `/Applications/VERGE-Qt` 
or on Linux `verged`/`verge-qt`.

The first time you run version 0.15.0 or newer, your chainstate database will be converted to a
new format, which will take anywhere from a few minutes to half an hour,
depending on the speed of your machine.


Downgrading warning
-------------------

Wallets created in 5.0.0 and later are not compatible with versions prior to 5.0.0
and will not work if you try to use newly created wallets in older versions. Existing
wallets that were created with older versions are not affected by this.

Compatibility
==============

VERGE is extensively tested on multiple operating systems using
the Linux kernel, macOS 10.8+, and Windows Vista and later. Windows XP is not supported.

VERGE should also work on most other Unix-like systems but is not
frequently tested on them.

0.16.2 change log
------------------

### Wallet
- #13622 `c04a4a5` Remove mapRequest tracking that just effects Qt display. (TheBlueMatt)
- #12905 `cfc6f74` [rpcwallet] Clamp walletpassphrase value at 100M seconds (sdaftuar)
- #13437 `ed82e71` wallet: Erase wtxOrderd wtx pointer on removeprunedfunds (MarcoFalke)

### RPC and other APIs
- #13451 `cbd2f70` rpc: expose CBlockIndex::nTx in getblock(header) (instagibbs)
- #13507 `f7401c8` RPC: Fix parameter count check for importpubkey (kristapsk)
- #13452 `6b9dc8c` rpc: have verifytxoutproof check the number of txns in proof structure (instagibbs)
- #12837 `bf1f150` rpc: fix type mistmatch in `listreceivedbyaddress` (joemphilips)
- #12743 `657dfc5` Fix csBestBlock/cvBlockChange waiting in rpc/mining (sipa)

### Build system
- #13544 `9fd3e00` depends: Update Qt download url (fanquake)
- #12573 `88d1a64` Fix compilation when compiler do not support `__builtin_clz*` (532479301)

### Tests and QA
- #13061 `170b309` Make tests pass after 2020 (bmwiedemann)
- #13192 `79c4fff` [tests] Fixed intermittent failure in `p2p_sendheaders.py` (lmanners)
- #13300 `d9c5630` qa: Initialize lockstack to prevent null pointer deref (MarcoFalke)
- #13545 `e15e3a9` tests: Fix test case `streams_serializedata_xor` Remove Boost dependency. (practicalswift)
- #13304 `cbdabef` qa: Fix `wallet_listreceivedby` race (MarcoFalke)

### Miscellaneous
- #12887 `2291774` Add newlines to end of log messages (jnewbery)
- #12859 `18b0c69` Bugfix: Include `<memory>` for `std::unique_ptr` (luke-jr)
- #13131 `ce8aa54` Add Windows shutdown handler (ken2812221)
- #13652 `20461fc` rpc: Fix that CWallet::AbandonTransaction would leave the grandchildren, etc. active (Empact)

Credits
=======

Thanks to everyone who directly contributed to this release:

- 532479301
- Ben Woosley
- Bernhard M. Wiedemann
- Chun Kuan Lee
- Cory Fields
- fanquake
- Gregory Sanders
- joemphilips
- John Newbery
- Kristaps Kaupe
- lmanners
- Luke Dashjr
- MarcoFalke
- Matt Corallo
- Pieter Wuille
- practicalswift
- Sjors Provoost
- Suhas Daftuar
- Wladimir J. van der Laan

And to those that reported security issues:

- Braydon Fuller
- Himanshu Mehta

As well as everyone that helped translating on [Transifex](https://www.transifex.com/projects/p/bitcoin/).
{% endgithubify %}

</div>
