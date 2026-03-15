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
qt_libexecdir="$QTDIR/libexec"
qt_plugindir="$QTDIR/plugins"
qt_translationdir="$QTDIR/translations"

boost_incdir="$BOOST_ROOT/include"
if [ ! -d "$boost_incdir/boost" ]; then
  versioned_boost_include="$(find "$boost_incdir" -mindepth 1 -maxdepth 1 -type d -name 'boost-*' | head -n1 || true)"
  if [ -n "$versioned_boost_include" ]; then
    boost_incdir="$versioned_boost_include"
  fi
fi
boost_libdir="${BOOST_LIBRARYDIR:-$BOOST_ROOT/lib}"
vcpkg_triplet="${VCPKG_DEFAULT_TRIPLET:-x64-windows-static-md}"
vcpkg_installed_dir="${VCPKG_INSTALLED_DIR:-${VCPKG_ROOT:-}/installed}"
vcpkg_triplet_dir=""
protoc_bindir=""
toolshim_root="$repo_root/build-msvc/toolchain/bin"
toolinclude_root="$repo_root/build-msvc/toolchain/include"
if [ -n "$vcpkg_installed_dir" ]; then
  vcpkg_triplet_dir="${vcpkg_installed_dir}/${vcpkg_triplet}"
fi

boost_lib_stem() {
  local component="$1"
  local libdir="$2"
  local match=""

  match="$(find "$libdir" -maxdepth 1 -type f \( -name "boost_${component}*.lib" -o -name "libboost_${component}*.lib" \) | sort | head -n1 || true)"
  if [ -z "$match" ]; then
    return 1
  fi

  local stem
  stem="$(basename "$match")"
  stem="${stem%.lib}"
  if [[ "$stem" == libboost_* ]]; then
    stem="${stem#lib}"
  fi
  printf '%s\n' "$stem"
}

if [ -n "${VERGE_MSVC_PKGCONFIG:-}" ]; then
  if [ -n "${PKG_CONFIG_PATH:-}" ]; then
    export PKG_CONFIG_PATH="${VERGE_MSVC_PKGCONFIG}:${PKG_CONFIG_PATH}"
  else
    export PKG_CONFIG_PATH="${VERGE_MSVC_PKGCONFIG}"
  fi
fi
export BOOST_ROOT
export BOOST_INCLUDEDIR="${BOOST_INCLUDEDIR:-$boost_incdir}"
export BOOST_LIBRARYDIR="$boost_libdir"
export CPPFLAGS="${CPPFLAGS:-} -I${toolinclude_root} -I${OPENSSL_ROOT_DIR}/include -I${BOOST_INCLUDEDIR}"
export LDFLAGS="${LDFLAGS:-} -L${OPENSSL_ROOT_DIR}/lib -L${BOOST_LIBRARYDIR}"
export CFLAGS="${CFLAGS:-} -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS"
export CXXFLAGS="${CXXFLAGS:-} -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -D_CRT_SECURE_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS"
if [ -n "$vcpkg_triplet_dir" ] && [ -d "$vcpkg_triplet_dir/include" ]; then
  export CPPFLAGS="${CPPFLAGS} -I${vcpkg_triplet_dir}/include"
fi
if [ -n "$vcpkg_triplet_dir" ] && [ -d "$vcpkg_triplet_dir/lib" ]; then
  export LDFLAGS="${LDFLAGS} -L${vcpkg_triplet_dir}/lib"
fi

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
mkdir -p "$toolshim_root"
mkdir -p "$toolinclude_root/sys"

make_qt_tool_shim() {
  local shim_name="$1"
  local tool_name="$2"
  local tool_path_win=""
  local candidate

  for candidate in \
    "$qt_libexecdir/${tool_name}.exe" \
    "$qt_libexecdir/$tool_name" \
    "$qt_bindir/${tool_name}.exe" \
    "$qt_bindir/$tool_name"
  do
    if [ -f "$candidate" ]; then
      tool_path_win="$candidate"
      break
    fi
  done

  if [ -z "$tool_path_win" ]; then
    return
  fi

  local tool_path_unix
  tool_path_unix="$(cygpath -u "$tool_path_win")"
  cat > "$toolshim_root/$shim_name" <<EOF
#!/usr/bin/env bash
exec "$tool_path_unix" "\$@"
EOF
  chmod +x "$toolshim_root/$shim_name"
}

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

make_qt_tool_shim moc moc
make_qt_tool_shim moc6 moc
make_qt_tool_shim moc-qt6 moc
make_qt_tool_shim uic uic
make_qt_tool_shim uic6 uic
make_qt_tool_shim uic-qt6 uic
make_qt_tool_shim rcc rcc
make_qt_tool_shim rcc6 rcc
make_qt_tool_shim rcc-qt6 rcc
make_qt_tool_shim lrelease lrelease
make_qt_tool_shim lrelease6 lrelease
make_qt_tool_shim lrelease-qt6 lrelease
make_qt_tool_shim lupdate lupdate
make_qt_tool_shim lupdate6 lupdate
make_qt_tool_shim lupdate-qt6 lupdate

chmod +x "$toolshim_root"/*

# Tor's configure probes include <sys/time.h> before <event2/event.h>, but the
# MSVC target does not provide that header. Supply a Windows-only shim for the
# probe and for Tor sources that expect timeval helpers on Windows.
cat > "$toolinclude_root/sys/time.h" <<'EOF'
#ifndef VERGE_WINDOWS_SYS_TIME_H
#define VERGE_WINDOWS_SYS_TIME_H

#include <winsock2.h>
#include <windows.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

struct timezone {
  int tz_minuteswest;
  int tz_dsttime;
};

static __inline int gettimeofday(struct timeval *tv, void *tz)
{
  FILETIME ft;
  ULARGE_INTEGER uli;
  unsigned long long usec;

  (void)tz;
  if (!tv) {
    return -1;
  }

  GetSystemTimeAsFileTime(&ft);
  uli.LowPart = ft.dwLowDateTime;
  uli.HighPart = ft.dwHighDateTime;
  usec = (uli.QuadPart - 116444736000000000ULL) / 10ULL;
  tv->tv_sec = (long)(usec / 1000000ULL);
  tv->tv_usec = (long)(usec % 1000000ULL);
  return 0;
}

#ifndef timerisset
#define timerisset(tvp) ((tvp)->tv_sec || (tvp)->tv_usec)
#endif

#ifndef timerclear
#define timerclear(tvp)       \
  do {                        \
    (tvp)->tv_sec = 0;        \
    (tvp)->tv_usec = 0;       \
  } while (0)
#endif

#ifndef timercmp
#define timercmp(a, b, CMP)                                            \
  (((a)->tv_sec == (b)->tv_sec) ?                                      \
   ((a)->tv_usec CMP (b)->tv_usec) :                                   \
   ((a)->tv_sec CMP (b)->tv_sec))
#endif

#ifndef timeradd
#define timeradd(a, b, result)                                         \
  do {                                                                 \
    (result)->tv_sec = (a)->tv_sec + (b)->tv_sec;                      \
    (result)->tv_usec = (a)->tv_usec + (b)->tv_usec;                   \
    if ((result)->tv_usec >= 1000000) {                                \
      ++(result)->tv_sec;                                              \
      (result)->tv_usec -= 1000000;                                    \
    }                                                                  \
  } while (0)
#endif

#ifndef timersub
#define timersub(a, b, result)                                         \
  do {                                                                 \
    (result)->tv_sec = (a)->tv_sec - (b)->tv_sec;                      \
    (result)->tv_usec = (a)->tv_usec - (b)->tv_usec;                   \
    if ((result)->tv_usec < 0) {                                       \
      --(result)->tv_sec;                                              \
      (result)->tv_usec += 1000000;                                    \
    }                                                                  \
  } while (0)
#endif

#ifdef __cplusplus
}
#endif

#endif
EOF

for search_root in \
  "${PROTOC_BINDIR:-}" \
  "${VCPKG_ROOT:-}" \
  "${vcpkg_installed_dir:-}"
do
  if [ -z "$search_root" ] || [ ! -d "$search_root" ]; then
    continue
  fi
  protoc_candidate="$(find "$search_root" -type f \( -name 'protoc.exe' -o -name 'protoc' \) | head -n1 || true)"
  if [ -n "$protoc_candidate" ]; then
    protoc_bindir="$(dirname "$protoc_candidate")"
    break
  fi
done

if [ -n "$protoc_bindir" ]; then
  protoc_candidate="$(find "$protoc_bindir" -maxdepth 1 -type f \( -name 'protoc.exe' -o -name 'protoc' \) | head -n1 || true)"
  if [ -n "$protoc_candidate" ]; then
    protoc_candidate_unix="$(cygpath -u "$protoc_candidate")"
    cat > "$toolshim_root/protoc" <<EOF
#!/usr/bin/env bash
exec "$protoc_candidate_unix" "\$@"
EOF
    chmod +x "$toolshim_root/protoc"
  fi
fi

export PATH="$toolshim_root:$PATH"
export CC="${CC:-$toolshim_root/cc-msvc}"
export CXX="${CXX:-$toolshim_root/cxx-msvc}"
export LD="${LD:-lld-link.exe}"
export AR="${AR:-$toolshim_root/ar-msvc}"
export NM="${NM:-$toolshim_root/nm-msvc}"
export RANLIB="${RANLIB:-:}"
export STRIP="${STRIP:-$toolshim_root/strip-msvc}"
export CONFIG_SITE=/dev/null
export TARGET_OS=windows
export LIBS="${LIBS:-} -levent_core -levent_extra -levent -liphlpapi -lbcrypt -lws2_32"

boost_filesystem_lib="$(boost_lib_stem filesystem "$BOOST_LIBRARYDIR")"
boost_program_options_lib="$(boost_lib_stem program_options "$BOOST_LIBRARYDIR")"
boost_thread_lib="$(boost_lib_stem thread "$BOOST_LIBRARYDIR")"
boost_chrono_lib="$(boost_lib_stem chrono "$BOOST_LIBRARYDIR")"
boost_system_lib="$(boost_lib_stem system "$BOOST_LIBRARYDIR")"
configure_protoc_args=()
if [ -n "$protoc_bindir" ]; then
  configure_protoc_args+=(--with-protoc-bindir="$protoc_bindir")
fi
configure_tor_dep_args=()
if [ -n "$vcpkg_triplet_dir" ] && [ -d "$vcpkg_triplet_dir" ]; then
  configure_tor_dep_args+=(--with-libevent-dir="$vcpkg_triplet_dir")
  configure_tor_dep_args+=(--with-zlib-dir="$vcpkg_triplet_dir")
fi
if [ -n "$OPENSSL_ROOT_DIR" ] && [ -d "$OPENSSL_ROOT_DIR" ]; then
  configure_tor_dep_args+=(--with-openssl-dir="$OPENSSL_ROOT_DIR")
fi

./autogen.sh

./configure \
  --build=x86_64-pc-windows \
  --host=x86_64-pc-windows \
  --enable-windows \
  --disable-bench \
  --disable-tests \
  --disable-dependency-tracking \
  --disable-hardening \
  --disable-asm \
  --with-boost="$BOOST_ROOT" \
  --with-boost-libdir="$BOOST_LIBRARYDIR" \
  --with-boost-filesystem="$boost_filesystem_lib" \
  --with-boost-program-options="$boost_program_options_lib" \
  --with-boost-thread="$boost_thread_lib" \
  --with-boost-chrono="$boost_chrono_lib" \
  --with-boost-system="$boost_system_lib" \
  "${configure_protoc_args[@]}" \
  "${configure_tor_dep_args[@]}" \
  --with-gui=qt6 \
  --with-qt-incdir="$qt_incdir" \
  --with-qt-libdir="$qt_libdir" \
  --with-qt-bindir="$qt_bindir" \
  --with-qt-plugindir="$qt_plugindir" \
  --with-qt-translationdir="$qt_translationdir" \
  BDB_CFLAGS="$BDB_CFLAGS" \
  BDB_LIBS="$BDB_LIBS"

make -j2 V=1
