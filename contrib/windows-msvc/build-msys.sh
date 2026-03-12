#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$repo_root"

: "${QTDIR:?QTDIR must be set}"
: "${BOOST_ROOT:?BOOST_ROOT must be set}"
: "${OPENSSL_ROOT_DIR:?OPENSSL_ROOT_DIR must be set}"
: "${BDB_CFLAGS:?BDB_CFLAGS must be set}"
: "${BDB_LIBS:?BDB_LIBS must be set}"

qt_incdir="$QTDIR/include"
qt_libdir="$QTDIR/lib"
qt_bindir="$QTDIR/bin"
qt_plugindir="$QTDIR/plugins"
qt_translationdir="$QTDIR/translations"

if [ -n "${VERGE_MSVC_PKGCONFIG:-}" ]; then
  if [ -n "${PKG_CONFIG_PATH:-}" ]; then
    export PKG_CONFIG_PATH="${VERGE_MSVC_PKGCONFIG};${PKG_CONFIG_PATH}"
  else
    export PKG_CONFIG_PATH="${VERGE_MSVC_PKGCONFIG}"
  fi
fi
export CPPFLAGS="${CPPFLAGS:-} -I${OPENSSL_ROOT_DIR}/include -I${BOOST_ROOT}/include"
export LDFLAGS="${LDFLAGS:-} -L${OPENSSL_ROOT_DIR}/lib -L${BOOST_ROOT}/lib"

# Use the Visual Studio LLVM driver with the MSVC ABI so the existing
# autotools flow can keep its Unix-like command line semantics.
export CC="${CC:-clang --target=x86_64-pc-windows-msvc}"
export CXX="${CXX:-clang++ --target=x86_64-pc-windows-msvc}"
export LD="${LD:-lld-link}"
export AR="${AR:-llvm-lib}"
export NM="${NM:-llvm-nm}"
export RANLIB="${RANLIB:-:}"
export STRIP="${STRIP:-llvm-strip}"

./autogen.sh

./configure \
  --enable-windows \
  --disable-bench \
  --disable-tests \
  --disable-dependency-tracking \
  --disable-hardening \
  --disable-asm \
  --with-gui=qt6 \
  --with-qt-incdir="$qt_incdir" \
  --with-qt-libdir="$qt_libdir" \
  --with-qt-bindir="$qt_bindir" \
  --with-qt-plugindir="$qt_plugindir" \
  --with-qt-translationdir="$qt_translationdir" \
  BDB_CFLAGS="$BDB_CFLAGS" \
  BDB_LIBS="$BDB_LIBS"

make -j2 V=1
