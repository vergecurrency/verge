macOS Build Instructions (macOS 14 / macOS 26)
==============================================

This document matches the macOS jobs in `.github/workflows/check-commit.yml`.

Prerequisites
-------------

Install Xcode command line tools:

```shell
xcode-select --install
```

Install Homebrew, then install dependencies:

```shell
brew tap-new verge/local || true
mkdir -p "$(brew --repo verge/local)/Formula"
curl -L https://raw.githubusercontent.com/vergecurrency/verge/refs/heads/master/depends/homebrew-formulas/boost176.rb \
  -o "$(brew --repo verge/local)/Formula/boost176.rb"
brew install verge/local/boost176

brew install automake autoconf pkg-config libtool \
  berkeley-db@4 zeromq miniupnpc \
  qt qtwebengine gperf qrencode librsvg \
  openssl@3 libevent

brew link qt berkeley-db@4 boost176
```

Build
-----

```shell
./autogen.sh

mkdir -p "$PWD/.qt-tool-aliases"
ln -sf "$(command -v moc)" "$PWD/.qt-tool-aliases/moc-qt6"
ln -sf "$(command -v uic)" "$PWD/.qt-tool-aliases/uic-qt6"
ln -sf "$(command -v rcc)" "$PWD/.qt-tool-aliases/rcc-qt6"

BREW_PREFIX="$(brew --prefix)"
BOOST_PREFIX="$(brew --prefix boost176)"
BDB_PREFIX="$(brew --prefix berkeley-db@4)"
OPENSSL_PREFIX="$(brew --prefix openssl@3)"
LIBEVENT_PREFIX="$(brew --prefix libevent)"

export CPPFLAGS="-I$BOOST_PREFIX/include -I$BDB_PREFIX/include -I$BREW_PREFIX/include ${CPPFLAGS:-}"
export LDFLAGS="-L$BOOST_PREFIX/lib -L$BDB_PREFIX/lib -L$BREW_PREFIX/lib ${LDFLAGS:-}"
export PKG_CONFIG_PATH="$BOOST_PREFIX/lib/pkgconfig:$BDB_PREFIX/lib/pkgconfig:$BREW_PREFIX/lib/pkgconfig:$BREW_PREFIX/share/pkgconfig:${PKG_CONFIG_PATH:-}"
export CXXFLAGS="-std=c++17 -D_LIBCPP_ENABLE_CXX17_REMOVED_UNARY_BINARY_FUNCTION -Wno-deprecated-builtins -Wno-deprecated-declarations"
export OBJCXXFLAGS="$CXXFLAGS"
export CFLAGS="-Wno-deprecated-builtins -Wno-deprecated-declarations"

./configure \
  --disable-tests \
  --disable-bench \
  --disable-werror \
  --with-gui=qt6 \
  --with-qt-bindir="$PWD/.qt-tool-aliases" \
  --with-boost="$BOOST_PREFIX" \
  --bindir="$PWD/release/bin" \
  --libdir="$PWD/release/lib" \
  --with-openssl-dir="$OPENSSL_PREFIX" \
  --with-libevent-dir="$LIBEVENT_PREFIX"

make -j4
```

Notes
-----

- CI resolves Qt host tools (`moc`, `uic`, `rcc`) from Homebrew Qt locations and provides aliases.
- If local configure cannot find Qt tools, add the following to `PATH`:

```shell
export PATH="$(brew --prefix qtbase)/bin:$(brew --prefix qtbase)/libexec:$(brew --prefix qt)/bin:$(brew --prefix qt)/libexec:$PATH"
```

- `verge-qt` binary output is `src/qt/verge-qt`.
