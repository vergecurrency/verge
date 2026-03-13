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

# Use the Visual Studio LLVM tools explicitly so autotools doesn't fall back to
# an MSYS/MinGW compiler or linker from PATH.
vs_llvm_root="${VCINSTALLDIR//\\//}/Tools/Llvm/x64/bin"
clang_bin="${vs_llvm_root}/clang.exe"
clangxx_bin="${vs_llvm_root}/clang++.exe"
llvm_ar_bin="${vs_llvm_root}/llvm-lib.exe"
llvm_nm_bin="${vs_llvm_root}/llvm-nm.exe"
llvm_strip_bin="${vs_llvm_root}/llvm-strip.exe"

if [ ! -x "$clang_bin" ] || [ ! -x "$clangxx_bin" ]; then
  echo "Visual Studio LLVM tools were not found under VCINSTALLDIR=$VCINSTALLDIR" >&2
  exit 1
fi

common_target_flags='--target=x86_64-pc-windows-msvc -fuse-ld=lld-link'
export CC="${CC:-${clang_bin} ${common_target_flags}}"
export CXX="${CXX:-${clangxx_bin} ${common_target_flags}}"
export LD="${LD:-lld-link.exe}"
export AR="${AR:-${llvm_ar_bin}}"
export NM="${NM:-${llvm_nm_bin}}"
export RANLIB="${RANLIB:-:}"
export STRIP="${STRIP:-${llvm_strip_bin}}"

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
