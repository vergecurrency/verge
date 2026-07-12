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

  match="$(find "$libdir" -maxdepth 1 -type f \( -name "boost_${component}*.lib" -o -name "libboost_${component}*.lib" \) ! -name "*-s-*" | sort | tail -n1 || true)"
  if [ -z "$match" ]; then
    match="$(find "$libdir" -maxdepth 1 -type f \( -name "boost_${component}*.lib" -o -name "libboost_${component}*.lib" \) | sort | tail -n1 || true)"
  fi
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
export CPPFLAGS="${CPPFLAGS:-} -DSECP256K1_STATIC -I${toolinclude_root} -I${OPENSSL_ROOT_DIR}/include -I${BOOST_INCLUDEDIR}"
export LDFLAGS="${LDFLAGS:-} -L${OPENSSL_ROOT_DIR}/lib -L${BOOST_LIBRARYDIR}"
win_system_libs=(
  -lws2_32
  -lshell32
  -lshlwapi
  -liphlpapi
  -lcrypt32
  -lbcrypt
  -ladvapi32
  -luser32
  -lgdi32
)
export LIBS="${LIBS:-} ${win_system_libs[*]}"
export CFLAGS="${CFLAGS:-} -fgnu89-inline -DWIN32 -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_AMD64_ -DBOOST_ALL_NO_LIB"
export CXXFLAGS="${CXXFLAGS:-} -DWIN32 -DWIN32_LEAN_AND_MEAN -D_WIN32_WINNT=0x0601 -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D_WINSOCK_DEPRECATED_NO_WARNINGS -D_AMD64_ -DBOOST_ALL_NO_LIB"
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
archive_bin_win="$llvm_ar_bin_win"
win_sdk_root="${WindowsSdkDir:-C:\\Program Files (x86)\\Windows Kits\\10\\}"
win_sdk_ver="${WindowsSDKVersion:-}"
rc_bin_win=""
cvtres_bin_win=""

clang_bin="$(cygpath -u "$clang_bin_win")"
clangxx_bin="$(cygpath -u "$clangxx_bin_win")"
llvm_nm_bin="$(cygpath -u "$llvm_nm_bin_win")"
llvm_strip_bin="$(cygpath -u "$llvm_strip_bin_win")"

if [ ! -x "$clang_bin" ] || [ ! -x "$clangxx_bin" ] || [ ! -x "$(cygpath -u "$llvm_ar_bin_win")" ]; then
  standalone_llvm_root="${LLVM_ROOT:-C:\\Program Files\\LLVM}\\bin"
  if [ -x "$(cygpath -u "${standalone_llvm_root//\\//}/clang.exe")" ] && \
     [ -x "$(cygpath -u "${standalone_llvm_root//\\//}/clang++.exe")" ] && \
     [ -x "$(cygpath -u "${standalone_llvm_root//\\//}/llvm-lib.exe")" ]; then
    clang_bin_win="${standalone_llvm_root//\\//}/clang.exe"
    clangxx_bin_win="${standalone_llvm_root//\\//}/clang++.exe"
    llvm_ar_bin_win="${standalone_llvm_root//\\//}/llvm-lib.exe"
    llvm_nm_bin_win="${standalone_llvm_root//\\//}/llvm-nm.exe"
    llvm_strip_bin_win="${standalone_llvm_root//\\//}/llvm-strip.exe"
    archive_bin_win="$llvm_ar_bin_win"
    clang_bin="$(cygpath -u "$clang_bin_win")"
    clangxx_bin="$(cygpath -u "$clangxx_bin_win")"
    llvm_nm_bin="$(cygpath -u "$llvm_nm_bin_win")"
    llvm_strip_bin="$(cygpath -u "$llvm_strip_bin_win")"
  fi
fi

if [ -n "${VCToolsInstallDir:-}" ]; then
  msvc_lib_bin_win="${VCToolsInstallDir//\\//}/bin/Hostx64/x64/lib.exe"
  msvc_lib_bin="$(cygpath -u "$msvc_lib_bin_win")"
  if [ -x "$msvc_lib_bin" ]; then
    archive_bin_win="$msvc_lib_bin_win"
  fi
  msvc_cvtres_bin_win="${VCToolsInstallDir//\\//}/bin/Hostx64/x64/cvtres.exe"
  msvc_cvtres_bin="$(cygpath -u "$msvc_cvtres_bin_win")"
  if [ -x "$msvc_cvtres_bin" ]; then
    cvtres_bin_win="$msvc_cvtres_bin_win"
  fi
fi

if [ -z "$win_sdk_ver" ]; then
  win_sdk_ver="$(find "${win_sdk_root//\\//}/bin" -mindepth 1 -maxdepth 1 -type d -name '10.*' | sort | tail -n1 | xargs -r basename)"
fi
if [ -n "$win_sdk_ver" ]; then
  candidate_rc_bin_win="${win_sdk_root//\\//}/bin/${win_sdk_ver}/x64/rc.exe"
  candidate_rc_bin="$(cygpath -u "$candidate_rc_bin_win")"
  if [ -x "$candidate_rc_bin" ]; then
    rc_bin_win="$candidate_rc_bin_win"
  fi
fi

archive_bin="$(cygpath -u "$archive_bin_win")"
rc_bin="$(cygpath -u "$rc_bin_win" 2>/dev/null || true)"
cvtres_bin="$(cygpath -u "$cvtres_bin_win" 2>/dev/null || true)"

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

msvc_compile_flags=(
  -fms-runtime-lib=dll
  -mpclmul
)

msvc_link_runtime_flags=(
  -Xlinker -nodefaultlib:libcmt
  -Xlinker -nodefaultlib:libucrt
  -Xlinker -defaultlib:msvcrt
  -Xlinker -defaultlib:ucrt
  -Xlinker -defaultlib:vcruntime
)

cat > "$toolshim_root/cc-msvc" <<EOF
#!/usr/bin/env bash
args=()
linking=1
for arg in "\$@"; do
  case "\$arg" in
    -c|-E|-S)
      linking=0
      args+=("\$arg")
      ;;
    -Wl,--large-address-aware|--large-address-aware)
      args+=(-Xlinker /LARGEADDRESSAWARE)
      ;;
    -Wl,--verge-protobuf-memswap16-alias|--verge-protobuf-memswap16-alias)
      args+=(-Xlinker '/alternatename:??\$memswap@\$0BA@@internal@protobuf@google@@YAXPEIAD0@Z=??\$memswap@\$0BA@@internal@protobuf@google@@YAXPEAD0@Z')
      ;;
    /WHOLEARCHIVE:*|/DEFAULTLIB:*|/NODEFAULTLIB:*|/ALTERNATENAME:*|/alternatename:*|/SUBSYSTEM:*|/subsystem:*)
      args+=(-Xlinker "\$arg")
      ;;
    *)
      args+=("\$arg")
      ;;
  esac
done
if [ "\$linking" -eq 1 ]; then
  exec "$clang_bin" --target=x86_64-pc-windows-msvc -fuse-ld=lld-link ${msvc_compile_flags[*]} ${msvc_link_runtime_flags[*]} "\${args[@]}"
else
  exec "$clang_bin" --target=x86_64-pc-windows-msvc -fuse-ld=lld-link ${msvc_compile_flags[*]} "\${args[@]}"
fi
EOF

cat > "$toolshim_root/cxx-msvc" <<EOF
#!/usr/bin/env bash
args=()
linking=1
for arg in "\$@"; do
  case "\$arg" in
    -c|-E|-S)
      linking=0
      args+=("\$arg")
      ;;
    -Wl,--large-address-aware|--large-address-aware)
      args+=(-Xlinker /LARGEADDRESSAWARE)
      ;;
    -Wl,--verge-protobuf-memswap16-alias|--verge-protobuf-memswap16-alias)
      args+=(-Xlinker '/alternatename:??\$memswap@\$0BA@@internal@protobuf@google@@YAXPEIAD0@Z=??\$memswap@\$0BA@@internal@protobuf@google@@YAXPEAD0@Z')
      ;;
    /WHOLEARCHIVE:*|/DEFAULTLIB:*|/NODEFAULTLIB:*|/ALTERNATENAME:*|/alternatename:*|/SUBSYSTEM:*|/subsystem:*)
      args+=(-Xlinker "\$arg")
      ;;
    *)
      args+=("\$arg")
      ;;
  esac
done
if [ "\$linking" -eq 1 ]; then
  exec "$clangxx_bin" --target=x86_64-pc-windows-msvc -fuse-ld=lld-link ${msvc_compile_flags[*]} ${msvc_link_runtime_flags[*]} "\${args[@]}"
else
  exec "$clangxx_bin" --target=x86_64-pc-windows-msvc -fuse-ld=lld-link ${msvc_compile_flags[*]} "\${args[@]}"
fi
EOF

cat > "$toolshim_root/ar-msvc" <<EOF
#!/usr/bin/env bash
set -euo pipefail

archive_tool="$archive_bin"
export MSYS2_ARG_CONV_EXCL='*'

extract_all_members() {
  local archive="\$1"
  local archive_native
  local member
  archive_native="\$(cygpath -aw "\$archive")"

  while IFS= read -r member; do
    [ -n "\$member" ] || continue
    "\$archive_tool" /nologo "/extract:\$member" "\$archive_native" >/dev/null
  done < <("\$archive_tool" /nologo /list "\$archive_native")
}

case "\${1:-}" in
  x)
    shift
    extract_all_members "\$1"
    ;;
  c|cr|rc|cru|rcu|r|ru)
    args=()
    rsp_files=()
    shift
    out="\$1"
    shift
    out_native="\$(cygpath -aw "\$out")"
    for arg in "\$@"; do
      if [[ "\$arg" == @* ]]; then
        rsp_src="\${arg#@}"
        rsp_tmp="\$(mktemp --suffix=.rsp)"
        while IFS= read -r rsp_member; do
          [ -n "\$rsp_member" ] || continue
          printf '%s\n' "\$(cygpath -aw "\$rsp_member")" >> "\$rsp_tmp"
        done < "\$rsp_src"
        rsp_files+=("\$rsp_tmp")
        args+=("@\$(cygpath -aw "\$rsp_tmp")")
      else
        args+=("\$(cygpath -aw "\$arg")")
      fi
    done
    trap 'rm -f "\${rsp_files[@]}"' EXIT
    exec "\$archive_tool" /nologo "/out:\$out_native" "\${args[@]}"
    ;;
  *)
    exec "\$archive_tool" "\$@"
    ;;
esac
EOF

cat > "$toolshim_root/ar-lib-msvc" <<EOF
#!/usr/bin/env bash
exec "$repo_root/src/tor/ar-lib" "$archive_bin" "\$@"
EOF

cat > "$toolshim_root/lib" <<EOF
#!/usr/bin/env bash
export MSYS2_ARG_CONV_EXCL='*'
exec "$archive_bin" "\$@"
EOF

cat > "$toolshim_root/nm-msvc" <<EOF
#!/usr/bin/env bash
exec "$llvm_nm_bin" "\$@"
EOF

cat > "$toolshim_root/strip-msvc" <<EOF
#!/usr/bin/env bash
exec "$llvm_strip_bin" "\$@"
EOF

if [ -n "$rc_bin" ] && [ -n "$cvtres_bin" ] && [ -x "$rc_bin" ] && [ -x "$cvtres_bin" ]; then
  cat > "$toolshim_root/windres" <<EOF
#!/usr/bin/env bash
set -euo pipefail

rc_tool="$rc_bin"
cvtres_tool="$cvtres_bin"
export MSYS2_ARG_CONV_EXCL='*'
args=()
input=""
output=""
tmp_res=""

while [ \$# -gt 0 ]; do
  case "\$1" in
    -D*)
      define="\${1#-D}"
      [ -n "\$define" ] && args+=("/d" "\$define")
      ;;
    -I*)
      include="\${1#-I}"
      [ -n "\$include" ] && args+=("/i" "\$(cygpath -aw "\$include")")
      ;;
    -i)
      shift
      input="\$1"
      ;;
    -o)
      shift
      output="\$1"
      ;;
    --input=*)
      input="\${1#--input=}"
      ;;
    --output=*)
      output="\${1#--output=}"
      ;;
  esac
  shift
done

[ -n "\$input" ] || { echo "windres shim: missing input" >&2; exit 1; }
[ -n "\$output" ] || { echo "windres shim: missing output" >&2; exit 1; }

input_native="\$(cygpath -aw "\$input")"
output_native="\$(cygpath -aw "\$output")"
tmp_res="\$(mktemp --suffix=.res)"
tmp_res_native="\$(cygpath -aw "\$tmp_res")"

"\$rc_tool" /nologo /fo"\$tmp_res_native" "\${args[@]}" "\$input_native"
"\$cvtres_tool" /NOLOGO /MACHINE:X64 /OUT:"\$output_native" "\$tmp_res_native"
rm -f "\$tmp_res"
EOF
  chmod +x "$toolshim_root/windres"
fi

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

for protoc_candidate in \
  "${PROTOC_BINDIR:-}/protoc.exe" \
  "${PROTOC_BINDIR:-}/protoc" \
  "${vcpkg_triplet_dir:-}/tools/protobuf/protoc.exe" \
  "${vcpkg_triplet_dir:-}/tools/protobuf/protoc" \
  "${vcpkg_installed_dir:-}/${vcpkg_triplet}/tools/protobuf/protoc.exe" \
  "${vcpkg_installed_dir:-}/${vcpkg_triplet}/tools/protobuf/protoc" \
  "${VCPKG_ROOT:-}/installed/${vcpkg_triplet}/tools/protobuf/protoc.exe" \
  "${VCPKG_ROOT:-}/installed/${vcpkg_triplet}/tools/protobuf/protoc"
do
  if [ -n "$protoc_candidate" ] && [ -f "$protoc_candidate" ]; then
    protoc_bindir="$(dirname "$protoc_candidate")"
    break
  fi
done

if [ -z "$protoc_bindir" ]; then
  for search_root in \
    "${vcpkg_installed_dir:-}" \
    "${VCPKG_ROOT:-}"
  do
    if [ -z "$search_root" ] || [ ! -d "$search_root" ]; then
      continue
    fi
    protoc_candidate="$(find "$search_root" -type f \( -path '*/tools/protobuf/protoc.exe' -o -path '*/tools/protobuf/protoc' \) | head -n1 || true)"
    if [ -n "$protoc_candidate" ]; then
      protoc_bindir="$(dirname "$protoc_candidate")"
      break
    fi
  done
fi

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
if [ -x "$toolshim_root/windres" ]; then
  export WINDRES="${WINDRES:-$toolshim_root/windres}"
fi
export CONFIG_SITE=/dev/null
export TARGET_OS=windows

boost_filesystem_lib="$(boost_lib_stem filesystem "$BOOST_LIBRARYDIR")"
boost_program_options_lib="$(boost_lib_stem program_options "$BOOST_LIBRARYDIR")"
boost_thread_lib="$(boost_lib_stem thread "$BOOST_LIBRARYDIR")"
boost_chrono_lib="$(boost_lib_stem chrono "$BOOST_LIBRARYDIR")"
boost_system_lib=""
if ! boost_system_lib="$(boost_lib_stem system "$BOOST_LIBRARYDIR")"; then
  boost_system_lib=""
fi
configure_protoc_args=()
if [ -n "$protoc_bindir" ]; then
  configure_protoc_args+=(--with-protoc-bindir="$protoc_bindir")
fi
configure_tor_dep_args=()
if [ -n "$vcpkg_triplet_dir" ] && [ -d "$vcpkg_triplet_dir" ]; then
  configure_tor_dep_args+=(--with-libevent-dir="$vcpkg_triplet_dir")
  configure_tor_dep_args+=(--with-zlib-dir="$vcpkg_triplet_dir")
fi
configure_boost_system_args=()
if [ -n "$boost_system_lib" ]; then
  configure_boost_system_args+=(--with-boost-system="$boost_system_lib")
fi
if [ -n "$OPENSSL_ROOT_DIR" ] && [ -d "$OPENSSL_ROOT_DIR" ]; then
  configure_tor_dep_args+=(--with-openssl-dir="$OPENSSL_ROOT_DIR")
fi

patch_tor_configure_for_windows() {
  local tor_config="$repo_root/src/tor/configure.ac"

  if grep -q 'TOR_OPENSSL_LIBS="-lssl -lcrypto $TOR_LIB_GDI $TOR_LIB_WS32 $TOR_LIB_CRYPT32 $TOR_LIB_BCRYPT -ladvapi32 -luser32"' "$tor_config" && \
     grep -q 'TOR_LIBEVENT_LIBS="$TOR_LIBEVENT_LIBS $TOR_LIB_IPHLPAPI $TOR_LIB_BCRYPT -ladvapi32 -lshell32 -luser32 $TOR_LIB_WS32"' "$tor_config"; then
    return
  fi

  perl -0pi -e 's#LIBS="\$STATIC_LIBEVENT_FLAGS \$TOR_LIB_WS32 \$save_LIBS"#case "\$host" in\n  *mingw*|*windows*)\n    LIBS="\$STATIC_LIBEVENT_FLAGS \$TOR_LIB_WS32 \$TOR_LIB_IPHLPAPI \$TOR_LIB_BCRYPT -ladvapi32 -lshell32 -luser32 \$save_LIBS"\n    ;;\n  *)\n    LIBS="\$STATIC_LIBEVENT_FLAGS \$TOR_LIB_WS32 \$save_LIBS"\n    ;;\nesac#' "$tor_config"

  perl -0pi -e 's#if test "\$ac_cv_search_evdns_base_new" != "none required"; then\n         TOR_LIBEVENT_LIBS="\$ac_cv_search_evdns_base_new \$TOR_LIBEVENT_LIBS"\n       fi#if test "\$ac_cv_search_evdns_base_new" != "none required"; then\n         TOR_LIBEVENT_LIBS="\$ac_cv_search_evdns_base_new \$TOR_LIBEVENT_LIBS"\n       fi\n       case "\$host" in\n         *mingw*|*windows*)\n           TOR_LIBEVENT_LIBS="\$TOR_LIBEVENT_LIBS \$TOR_LIB_IPHLPAPI \$TOR_LIB_BCRYPT -ladvapi32 -lshell32 -luser32 \$TOR_LIB_WS32"\n           ;;\n       esac#' "$tor_config"

  perl -0pi -e 's#TOR_SEARCH_LIBRARY\(openssl, \$tryssldir, \[-lssl -lcrypto \$TOR_LIB_GDI \$TOR_LIB_WS32 \$TOR_LIB_CRYPT32\],#TOR_SEARCH_LIBRARY(openssl, \$tryssldir, [-lssl -lcrypto \$TOR_LIB_GDI \$TOR_LIB_WS32 \$TOR_LIB_CRYPT32 \$TOR_LIB_BCRYPT -ladvapi32 -luser32],#' "$tor_config"

  perl -0pi -e 's#TOR_OPENSSL_LIBS="-lssl -lcrypto"#case "\$host" in\n  *mingw*|*windows*)\n     TOR_OPENSSL_LIBS="-lssl -lcrypto \$TOR_LIB_GDI \$TOR_LIB_WS32 \$TOR_LIB_CRYPT32 \$TOR_LIB_BCRYPT -ladvapi32 -luser32"\n     ;;\n  *)\n     TOR_OPENSSL_LIBS="-lssl -lcrypto"\n     ;;\nesac#' "$tor_config"

  grep -n 'TOR_LIBEVENT_LIBS=' "$tor_config" >/dev/null
}

patch_tor_sources_for_windows() {
  local control_getinfo="$repo_root/src/tor/src/feature/control/control_getinfo.c"
  local compat_compiler="$repo_root/src/tor/src/lib/cc/compat_compiler.h"
  local trunnel_header="$repo_root/src/tor/src/ext/trunnel/trunnel.h"
  local hashx_program_exec="$repo_root/src/tor/src/ext/equix/hashx/src/program_exec.c"

  if grep -q '#include <process.h>' "$control_getinfo"; then
    :
  else
    perl -0pi -e 's/#ifdef HAVE_UNISTD_H/#ifdef _WIN32\n#include <process.h>\n#endif\n\n#ifdef HAVE_UNISTD_H/' "$control_getinfo"
    grep -n '#include <process.h>' "$control_getinfo" >/dev/null || {
      echo "failed to patch $control_getinfo with process.h" >&2
      sed -n '60,90p' "$control_getinfo" >&2
      exit 1
    }
  fi

  if ! grep -q 'VERGE_MSVC_WINDOWS_CRT_COMPAT' "$compat_compiler"; then
    perl -0pi -e 's/#include <inttypes\.h>/#include <inttypes.h>\n\n#ifdef _WIN32\n#include <direct.h>\n#include <io.h>\n#include <process.h>\n#include <string.h>\n#include <sys\/time.h>\n#include <sys\/utime.h>\n\n\/\* VERGE_MSVC_WINDOWS_CRT_COMPAT: map POSIX CRT names used by Tor sources to\n \* the MSVC spellings without affecting configure probes.\n \*\/\n#ifndef HAVE_STRCASECMP\n#define HAVE_STRCASECMP 1\n#endif\n#ifndef HAVE_STRNCASECMP\n#define HAVE_STRNCASECMP 1\n#endif\n#ifndef strcasecmp\n#define strcasecmp _stricmp\n#endif\n#ifndef strncasecmp\n#define strncasecmp _strnicmp\n#endif\n#ifndef utime\n#define utime _utime\n#endif\n#ifndef STDIN_FILENO\n#define STDIN_FILENO 0\n#endif\n#ifndef STDOUT_FILENO\n#define STDOUT_FILENO 1\n#endif\n#ifndef STDERR_FILENO\n#define STDERR_FILENO 2\n#endif\n#ifndef getcwd\n#define getcwd _getcwd\n#endif\n#ifndef fileno\n#define fileno _fileno\n#endif\n#ifndef unlink\n#define unlink _unlink\n#endif\n#endif/' "$compat_compiler"
  else
    if ! grep -q '#include <process.h>' "$compat_compiler"; then
      perl -0pi -e 's/#include <io\.h>/#include <io.h>\n#include <process.h>/' "$compat_compiler"
    fi
    if ! grep -q '#include <string.h>' "$compat_compiler"; then
      perl -0pi -e 's/#include <process\.h>/#include <process.h>\n#include <string.h>/' "$compat_compiler"
    fi
    if ! grep -q '#include <sys/time.h>' "$compat_compiler"; then
      perl -0pi -e 's/#include <string\.h>/#include <string.h>\n#include <sys\/time.h>/' "$compat_compiler"
    fi
    if ! grep -q '#include <sys/utime.h>' "$compat_compiler"; then
      perl -0pi -e 's/#include <sys\/time\.h>/#include <sys\/time.h>\n#include <sys\/utime.h>/' "$compat_compiler"
    fi
    if ! grep -q '#define strcasecmp _stricmp' "$compat_compiler"; then
      perl -0pi -e 's/\/\* VERGE_MSVC_WINDOWS_CRT_COMPAT: map POSIX CRT names used by Tor sources to\n \* the MSVC spellings without affecting configure probes.\n \*\/\n/\/\* VERGE_MSVC_WINDOWS_CRT_COMPAT: map POSIX CRT names used by Tor sources to\n \* the MSVC spellings without affecting configure probes.\n \*\/\n#ifndef HAVE_STRCASECMP\n#define HAVE_STRCASECMP 1\n#endif\n#ifndef HAVE_STRNCASECMP\n#define HAVE_STRNCASECMP 1\n#endif\n#ifndef strcasecmp\n#define strcasecmp _stricmp\n#endif\n#ifndef strncasecmp\n#define strncasecmp _strnicmp\n#endif\n/' "$compat_compiler"
    fi
    if ! grep -q '#define utime _utime' "$compat_compiler"; then
      perl -0pi -e 's/#ifndef strncasecmp\n#define strncasecmp _strnicmp\n#endif/#ifndef strncasecmp\n#define strncasecmp _strnicmp\n#endif\n#ifndef utime\n#define utime _utime\n#endif/' "$compat_compiler"
    fi
  fi

  if ! grep -q 'lib/cc/torint.h' "$trunnel_header"; then
    perl -0pi -e 's/#include <sys\/types\.h>/#include <sys\/types.h>\n#include "lib\/cc\/torint.h"/' "$trunnel_header"
  fi

  perl -0pi -e 's/#if EVAL_DEFINE\(__MACHINEARM64_X64\(1\)\)/#if defined(_M_ARM64EC)/g' "$hashx_program_exec"
  perl -0pi -e 's/#if EVAL_DEFINE\(__MACHINEARM64_X64\(1\)\)\s*\|\|\s*defined\(_M_ARM64EC\)/#if defined(_M_ARM64EC)/g' "$hashx_program_exec"
  perl -0pi -e 's/#if defined\(_M_ARM64EC\)\s*\|\|\s*\(defined\(__MACHINEARM64_X64\)\s*&&\s*EVAL_DEFINE\(__MACHINEARM64_X64\(1\)\)\)/#if defined(_M_ARM64EC)/g' "$hashx_program_exec"
  perl -0pi -e 's/#if EVAL_DEFINE\(__MACHINEX64\(1\)\)/#if defined(_M_X64)/g' "$hashx_program_exec"
  perl -0pi -e 's/#if EVAL_DEFINE\(__MACHINEX64\(1\)\)\s*\|\|\s*defined\(_M_X64\)/#if defined(_M_X64)/g' "$hashx_program_exec"
  perl -0pi -e 's/#if defined\(_M_X64\)\s*\|\|\s*\(defined\(__MACHINEX64\)\s*&&\s*EVAL_DEFINE\(__MACHINEX64\(1\)\)\)/#if defined(_M_X64)/g' "$hashx_program_exec"
  perl -0pi -e 's/#if defined\(_M_X64\)\r?\nstatic FORCE_INLINE int64_t smulh\(int64_t a, int64_t b\) \{/#if defined(_M_X64) \&\& !defined(HAVE_SMULH)\nstatic FORCE_INLINE int64_t smulh(int64_t a, int64_t b) {/g' "$hashx_program_exec"

  grep -n 'lib/cc/torint.h' "$trunnel_header" >/dev/null || {
    echo "failed to patch $trunnel_header for torint.h" >&2
    sed -n '1,25p' "$trunnel_header" >&2
    exit 1
  }
  grep -n '#if defined(_M_X64)' "$hashx_program_exec" >/dev/null || {
    echo "failed to patch $hashx_program_exec for _M_X64 guards" >&2
    sed -n '20,70p' "$hashx_program_exec" >&2
    exit 1
  }
  grep -n 'VERGE_MSVC_WINDOWS_CRT_COMPAT' "$compat_compiler" >/dev/null || {
    echo "failed to patch $compat_compiler for MSVC CRT compatibility" >&2
    sed -n '1,80p' "$compat_compiler" >&2
    exit 1
  }
}

patch_tor_build_helpers_for_windows() {
  local combine_libs="$repo_root/src/tor/scripts/build/combine_libs"
  cat > "$combine_libs" <<'EOF'
#!/bin/sh

set -e

abspath() {
    echo "$(cd "$(dirname "$1")" >/dev/null && pwd)/$(basename "$1")"
}

run_ar() {
    set -- ${AR:-ar} "$@"
    "$@"
}

TARGET=$(abspath "$1")

shift

inputs=""
for input in "$@"; do
    inputs="$inputs $(abspath "$input")"
done

# MSVC lib.exe can merge COFF archives directly, so avoid unpack/repack logic.
eval "run_ar \"\${ARFLAGS:-cru}\" \"\$TARGET\"$inputs"
EOF
  chmod +x "$combine_libs"

  grep -n 'run_ar() {' "$combine_libs" >/dev/null
  grep -n 'MSVC lib.exe can merge COFF archives directly' "$combine_libs" >/dev/null
  grep -n 'eval "run_ar' "$combine_libs" >/dev/null
}

patch_verge_sources_for_windows() {
  local httpserver_cpp="$repo_root/src/httpserver.cpp"
  local tradepage_cpp="$repo_root/src/qt/tradepage.cpp"
  local verge_qt_cpp="$repo_root/src/qt/verge.cpp"

  perl -0pi -e 's/evhttp_connection_get_peer\(con, \(char\*\*\)&address, &port\);/evhttp_connection_get_peer(con, \&address, \&port);/' "$httpserver_cpp"
  perl -0pi -e 's/#include <QWebEnginePage>/#include <QtWebEngineCore\/QWebEnginePage>/' "$tradepage_cpp"
  perl -0pi -e 's/#include <QWebEngineView>/#include <QtWebEngineWidgets\/QWebEngineView>/' "$tradepage_cpp"
  if ! grep -q 'VERGE_MSVC_PROTOBUF_MEMSWAP_ALIAS' "$verge_qt_cpp"; then
    perl -0pi -e 's/#include <qt\/vergegui\.h>\r?\n/#include <qt\/vergegui.h>\n\n#if defined(_WIN32) \&\& defined(__clang__)\n#pragma comment(linker, "\/alternatename:??\$memswap@\$0BA@@internal@protobuf@google@@YAXPEIAD0@Z=??\$memswap@\$0BA@@internal@protobuf@google@@YAXPEAD0@Z")\n#endif\n\/\* VERGE_MSVC_PROTOBUF_MEMSWAP_ALIAS \*\/\n/' "$verge_qt_cpp"
  fi

  grep -n 'evhttp_connection_get_peer(con, &address, &port);' "$httpserver_cpp" >/dev/null
  grep -n '#include <QtWebEngineCore/QWebEnginePage>' "$tradepage_cpp" >/dev/null
  grep -n '#include <QtWebEngineWidgets/QWebEngineView>' "$tradepage_cpp" >/dev/null
  grep -n 'VERGE_MSVC_PROTOBUF_MEMSWAP_ALIAS' "$verge_qt_cpp" >/dev/null
}

patch_pow_sources_for_windows() {
  local sponge_c="$repo_root/src/crypto/pow/Sponge.c"

  perl -0pi -e 's/^inline void /void /mg' "$sponge_c"

  grep -n '^void initState' "$sponge_c" >/dev/null
  grep -n '^void squeeze' "$sponge_c" >/dev/null
  grep -n '^void absorbBlock' "$sponge_c" >/dev/null
  grep -n '^void reducedSqueezeRow0' "$sponge_c" >/dev/null
}

echo "==> Initializing submodules"
git -c submodule.recurse=false submodule update --init -- src/tor src/secp256k1 src/leveldb
echo "==> Patching tor configure for Windows MSVC"
patch_tor_configure_for_windows
echo "==> Patching tor sources for Windows MSVC"
patch_tor_sources_for_windows
echo "==> Patching tor build helpers for Windows MSVC"
patch_tor_build_helpers_for_windows
echo "==> Patching Verge sources for Windows MSVC"
patch_verge_sources_for_windows
echo "==> Patching PoW sources for Windows MSVC"
patch_pow_sources_for_windows

./autogen.sh

./configure \
  --build=x86_64-pc-windows \
  --host=x86_64-pc-windows \
  --enable-windows \
  --disable-shared \
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
  "${configure_boost_system_args[@]}" \
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

patch_generated_makefiles_for_windows() {
  local top_makefile="$repo_root/src/Makefile"
  local protobuf_static_libs=""
  local protobuf_link_libs=""
  local protobuf_makefile_libs=""
  local protobuf_memswap_alias_flags=""
  local protobuf_member_obj=""
  local protobuf_extract_dir=""
  local protobuf_extract_tmp=""
  local protobuf_extracted_obj=""
  local lib
  local stem

  if [ -n "$vcpkg_triplet_dir" ] && [ -d "$vcpkg_triplet_dir/lib" ]; then
    if [ -f "$vcpkg_triplet_dir/lib/protobuf.lib" ]; then
      protobuf_member_obj="$repo_root/build-msvc/protobuf/repeated_ptr_field.cc.obj"
      if [ ! -f "$protobuf_member_obj" ]; then
        protobuf_extract_dir="$(dirname "$protobuf_member_obj")"
        protobuf_extract_tmp="$(mktemp -d)"
        mkdir -p "$protobuf_extract_dir"
        (
          cd "$protobuf_extract_tmp"
          "$archive_bin" /nologo "/extract:CMakeFiles\\libprotobuf.dir\\src\\google\\protobuf\\repeated_ptr_field.cc.obj" "$(cygpath -aw "$vcpkg_triplet_dir/lib/protobuf.lib")" >/dev/null
        )
        protobuf_extracted_obj="$(find "$protobuf_extract_tmp" -type f -name 'repeated_ptr_field.cc.obj' | head -n1 || true)"
        if [ -z "$protobuf_extracted_obj" ]; then
          echo "Failed to extract repeated_ptr_field.cc.obj from protobuf.lib" >&2
          exit 1
        fi
        cp "$protobuf_extracted_obj" "$protobuf_member_obj"
        rm -rf "$protobuf_extract_tmp"
      fi
      protobuf_link_libs+=" $(cygpath -am "$protobuf_member_obj") -lprotobuf"
    elif [ -f "$vcpkg_triplet_dir/lib/protobuf-lite.lib" ]; then
      protobuf_link_libs+=" -Wl,/WHOLEARCHIVE:${vcpkg_triplet_dir}/lib/protobuf-lite.lib -lprotobuf-lite"
    fi
    for lib in "$vcpkg_triplet_dir"/lib/absl_*.lib "$vcpkg_triplet_dir"/lib/utf8*.lib; do
      [ -f "$lib" ] || continue
      stem="$(basename "$lib" .lib)"
      protobuf_static_libs+=" -l${stem}"
    done
  fi

  perl -0pi -e 's/^LIBSECP256K1 = secp256k1\/libsecp256k1\.la$/LIBSECP256K1 = secp256k1\/libsecp256k1.la secp256k1\/libsecp256k1_precomputed.la/m' "$top_makefile"
  protobuf_memswap_alias_flags=' -Wl,--verge-protobuf-memswap16-alias'
  protobuf_makefile_libs="${protobuf_link_libs}${protobuf_static_libs}${protobuf_memswap_alias_flags}"
  if [ -n "$protobuf_link_libs" ] || [ -n "$protobuf_static_libs" ]; then
    PROTOBUF_MAKEFILE_LIBS="$protobuf_makefile_libs" \
      perl -0pi -e 's#^PROTOBUF_LIBS = .*$#"PROTOBUF_LIBS = $ENV{PROTOBUF_MAKEFILE_LIBS}"#me' "$top_makefile"
  fi
  perl -0pi -e 's#^qt_verge_qt_LDFLAGS = (.*)$#qt_verge_qt_LDFLAGS = $1 -Wl,/subsystem:windows#m' "$top_makefile"
  perl -0pi -e 's/-lQt6Widgets/-lQt6EntryPoint -lQt6Widgets/' "$top_makefile"
  perl -0pi -e 's/-lQt6WebEngineWidgets/-lQt6WebEngineWidgets -lQt6WebEngineCore/' "$top_makefile"

  grep -n '^LIBSECP256K1 = secp256k1/libsecp256k1.la secp256k1/libsecp256k1_precomputed.la$' "$top_makefile" >/dev/null
  grep -n '^PROTOBUF_LIBS = .*repeated_ptr_field\.cc\.obj .*--verge-protobuf-memswap16-alias' "$top_makefile" >/dev/null
  grep -n '^qt_verge_qt_LDFLAGS = .* -Wl,/subsystem:windows$' "$top_makefile" >/dev/null
  grep -n 'Qt6EntryPoint -lQt6Widgets' "$top_makefile" >/dev/null
  grep -n 'Qt6WebEngineWidgets -lQt6WebEngineCore' "$top_makefile" >/dev/null
}

patch_generated_makefiles_for_windows

make -j1 V=1
