---
# This file is licensed under the MIT License (MIT) available on
# http://opensource.org/licenses/MIT.
# Text originally from Bitcoin Core project
# Metadata and small formatting changes from Bitcoin.org project

## Required value below populates the %v variable (note: % needs to be escaped in YAML if it starts a value)
required_version: 0.16.3
## Required title.
title: Bitcoin Core version 0.16.3 released
## Optional release date.  May be filled in hours/days after a release
optional_date: 2018-09-28

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


0.16.3 change log
------------------

### Consensus
- #14249 `696b936` Fix crash bug with duplicate inputs within a transaction (TheBlueMatt, sdaftuar)

### RPC and other APIs
- #13547 `212ef1f` Make `signrawtransaction*` give an error when amount is needed but missing (ajtowns)

### Miscellaneous
- #13655 `1cdbea7` bitcoinconsensus: invalid flags error should be set to `bitcoinconsensus_err` (afk11)

### Documentation
- #13844 `11b9dbb` correct the help output for -prune (hebasto)

Credits
=======

Thanks to everyone who directly contributed to this release:

- Anthony Towns
- Hennadii Stepanov
- Matt Corallo
- Suhas Daftuar
- Thomas Kerin
- Wladimir J. van der Laan

And to those that reported security issues:

- (anonymous reporter)

</div>