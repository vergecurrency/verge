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
clang_bin_win="${vs_llvm_root}/clang.exe"
clangxx_bin_win="${vs_llvm_root}/clang++.exe"
llvm_ar_bin_win="${vs_llvm_root}/llvm-lib.exe"
llvm_nm_bin_win="${vs_llvm_root}/llvm-nm.exe"
llvm_strip_bin_win="${vs_llvm_root}/llvm-strip.exe"

clang_bin="$(cygpath -u "$clang_bin_win")"
clangxx_bin="$(cygpath -u "$clangxx_bin_win")"
llvm_ar_bin="$(cygpath -u "$llvm_ar_bin_win")"
llvm_nm_bin="$(cygpath -u "$llvm_nm_bin_win")"
llvm_strip_bin="$(cygpath -u "$llvm_strip_bin_win")"

if [ ! -x "$clang_bin" ] || [ ! -x "$clangxx_bin" ]; then
  echo "Visual Studio LLVM tools were not found under VCINSTALLDIR=$VCINSTALLDIR" >&2
  exit 1
fi

# Autotools stores CC/CXX as shell words, so a compiler path under
# /c/Program Files/... must be wrapped in a no-space shim.
toolshim_root="$repo_root/build-msvc/toolchain/bin"
mkdir -p "$toolshim_root"

cat > "$toolshim_root/cc-msvc" <<EOF
#!/usr/bin/env bash
exec "$clang_bin" --target=x86_64-pc-windows-msvc -fuse-ld=lld-link "\$@"
EOF

cat > "$toolshim_root/cxx-msvc" <<EOF
#!/usr/bin/env bash
exec "$clangxx_bin" --target=x86_64-pc-windows-msvc -fuse-ld=lld-link "\$@"
EOF

cat > "$toolshim_root/ar-msvc" <<EOF
#!/usr/bin/env bash
exec "$llvm_ar_bin" "\$@"
EOF

cat > "$toolshim_root/nm-msvc" <<EOF
#!/usr/bin/env bash
exec "$llvm_nm_bin" "\$@"
EOF

cat > "$toolshim_root/strip-msvc" <<EOF
#!/usr/bin/env bash
exec "$llvm_strip_bin" "\$@"
EOF

chmod +x "$toolshim_root"/*

export PATH="$toolshim_root:$PATH"
export CC="${CC:-$toolshim_root/cc-msvc}"
export CXX="${CXX:-$toolshim_root/cxx-msvc}"
export LD="${LD:-lld-link.exe}"
export AR="${AR:-$toolshim_root/ar-msvc}"
export NM="${NM:-$toolshim_root/nm-msvc}"
export RANLIB="${RANLIB:-:}"
export STRIP="${STRIP:-$toolshim_root/strip-msvc}"

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
