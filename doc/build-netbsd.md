NetBSD build guide
======================
(updated for NetBSD 7.0)

This guide describes how to build verged and command-line utilities on NetBSD.

This guide does not contain instructions for building the GUI.

Preparation
-------------

You will need the following modules, which can be installed via pkgsrc or pkgin:

```
autoconf
automake
boost
db4
git
gmake
libevent
libtool
python27
```

Download the source code:
```
git clone https://github.com/vergecurrency/VERGE
```

See [dependencies.md](dependencies.md) for a complete overview.

### Building Verge Core

**Important**: Use `gmake` (the non-GNU `make` will exit with an error).

To build without Tor, pass `--without-tor` to configure.

With wallet:
```
./autogen.sh
./configure CPPFLAGS="-I/usr/pkg/include" LDFLAGS="-L/usr/pkg/lib" BOOST_CPPFLAGS="-I/usr/pkg/include" BOOST_LDFLAGS="-L/usr/pkg/lib"
gmake
```

Without wallet:
```
./autogen.sh
./configure --disable-wallet CPPFLAGS="-I/usr/pkg/include" LDFLAGS="-L/usr/pkg/lib" BOOST_CPPFLAGS="-I/usr/pkg/include" BOOST_LDFLAGS="-L/usr/pkg/lib"
gmake
```
