PACKAGE=qt
$(package)_version=5.9.9
$(package)_download_path=https://download.qt.io/new_archive/qt/5.9/$($(package)_version)/submodules
$(package)_suffix=opensource-src-$($(package)_version).tar.xz
$(package)_file_name=qtbase-$($(package)_suffix)
$(package)_sha256_hash=d5a97381b9339c0fbaf13f0c05d599a5c999dcf94145044058198987183fed65
$(package)_dependencies=openssl zlib
$(package)_linux_dependencies=freetype fontconfig libxcb libX11 xorgproto libXext
$(package)_build_subdir=qtbase
$(package)_qt_libs=corelib network widgets gui plugins testlib
$(package)_patches=fix_qt_pkgconfig.patch mac-qmake.conf fix_limits_header.patch fix_configure_mac.patch fix_no_printer.patch fix_rcc_determinism.patch fix_riscv64_arch.patch xkb-default.patch fix_mingw_cross_compile.patch

$(package)_qttranslations_file_name=qttranslations-$($(package)_suffix)
$(package)_qttranslations_sha256_hash=f7474f260a1382549720081bf2359a3d425ec3bf7d31976c512834303d30d73b

$(package)_qttools_file_name=qttools-$($(package)_suffix)
$(package)_qttools_sha256_hash=fce6e0fd39a40bcef880c669080087dba94af1ec442296222210472e0852bf98

$(package)_extra_sources  = $($(package)_qttranslations_file_name)
$(package)_extra_sources += $($(package)_qttools_file_name)

define $(package)_set_vars
$(package)_config_opts_release = -release
$(package)_config_opts_debug = -debug
$(package)_config_opts += -bindir $(build_prefix)/bin
$(package)_config_opts += -c++std c++1z
$(package)_config_opts += -confirm-license
$(package)_config_opts += -no-dbus
$(package)_config_opts += -hostprefix $(build_prefix)
$(package)_config_opts += -no-compile-examples
$(package)_config_opts += -no-cups
$(package)_config_opts += -no-direct2d
$(package)_config_opts += -no-egl
$(package)_config_opts += -no-eglfs
$(package)_config_opts += -no-freetype
$(package)_config_opts += -no-gif
$(package)_config_opts += -no-glib
$(package)_config_opts += -no-icu
$(package)_config_opts += -no-iconv
$(package)_config_opts += -no-kms
$(package)_config_opts += -no-linuxfb
$(package)_config_opts += -no-libjpeg
$(package)_config_opts += -no-libudev
$(package)_config_opts += -no-mtdev
$(package)_config_opts += -no-openvg
$(package)_config_opts += -no-reduce-relocations
$(package)_config_opts += -no-qml-debug
$(package)_config_opts += -no-sql-db2
$(package)_config_opts += -no-sql-ibase
$(package)_config_opts += -no-sql-oci
$(package)_config_opts += -no-sql-tds
$(package)_config_opts += -no-sql-mysql
$(package)_config_opts += -no-sql-odbc
$(package)_config_opts += -no-sql-psql
$(package)_config_opts += -no-sql-sqlite
$(package)_config_opts += -no-sql-sqlite2
$(package)_config_opts += -no-use-gold-linker
$(package)_config_opts += -no-xinput2
$(package)_config_opts += -nomake examples
$(package)_config_opts += -nomake tests
$(package)_config_opts += -opensource
$(package)_config_opts += -openssl-linked
$(package)_config_opts += -optimized-qmake
$(package)_config_opts += -pch
$(package)_config_opts += -pkg-config
$(package)_config_opts += -prefix $(host_prefix)
$(package)_config_opts += -qt-libpng
$(package)_config_opts += -gif
$(package)_config_opts += -qt-pcre
$(package)_config_opts += -qt-harfbuzz
$(package)_config_opts += -system-zlib
$(package)_config_opts += -static
$(package)_config_opts += -silent
$(package)_config_opts += -v
$(package)_config_opts += -no-feature-dial
$(package)_config_opts += -no-feature-ftp
$(package)_config_opts += -no-feature-lcdnumber
$(package)_config_opts += -no-feature-pdf
$(package)_config_opts += -no-feature-printer
$(package)_config_opts += -no-feature-printdialog
$(package)_config_opts += -no-feature-concurrent
$(package)_config_opts += -no-feature-sql
$(package)_config_opts += -no-feature-statemachine
$(package)_config_opts += -no-feature-syntaxhighlighter
$(package)_config_opts += -no-feature-textbrowser
$(package)_config_opts += -no-feature-textodfwriter
$(package)_config_opts += -no-feature-udpsocket
$(package)_config_opts += -no-feature-wizard
$(package)_config_opts += -no-feature-xml
$(package)_config_opts += -Wno-deprecated-copy

ifneq ($(build_os),darwin)
$(package)_config_opts_darwin = -no-dbus
$(package)_config_opts_darwin = -xplatform macx-clang-linux
$(package)_config_opts_darwin += -device-option MAC_SDK_PATH=$(OSX_SDK)
$(package)_config_opts_darwin += -device-option MAC_SDK_VERSION=$(OSX_SDK_VERSION)
$(package)_config_opts_darwin += -device-option CROSS_COMPILE="$(host)-"
$(package)_config_opts_darwin += -device-option MAC_MIN_VERSION=$(OSX_MIN_VERSION)
$(package)_config_opts_darwin += -device-option MAC_TARGET=$(host)
$(package)_config_opts_darwin += -device-option MAC_LD64_VERSION=$(LD64_VERSION)
endif

$(package)_config_opts_linux  = -qt-xkbcommon-x11
$(package)_config_opts_linux += -qt-xcb
$(package)_config_opts_linux += -system-freetype
$(package)_config_opts_linux += -no-feature-sessionmanager
$(package)_config_opts_linux += -fontconfig
$(package)_config_opts_linux += -no-opengl
$(package)_config_opts_linux += -no-dbus
$(package)_config_opts_arm_linux += -platform linux-g++ -xplatform bitcoin-linux-g++
$(package)_config_opts_i686_linux  = -xplatform linux-g++-32
$(package)_config_opts_x86_64_linux = -xplatform linux-g++-64
$(package)_config_opts_aarch64_linux = -xplatform linux-aarch64-gnu-g++
$(package)_config_opts_riscv64_linux = -platform linux-g++ -xplatform bitcoin-linux-g++
$(package)_config_opts_mingw32  = -no-opengl -xplatform win32-g++ -device-option CROSS_COMPILE="$(host)-"
$(package)_build_env  = QT_RCC_TEST=1
$(package)_build_env += QT_RCC_SOURCE_DATE_OVERRIDE=1
endef

$(package)_config_opts_mingw32 = -no-opengl
$(package)_config_opts_mingw32 += -no-dbus
$(package)_config_opts_mingw32 += -no-freetype
$(package)_config_opts_mingw32 += -xplatform win32-g++
$(package)_config_opts_mingw32 += "QMAKE_CFLAGS = '$($(package)_cflags) $($(package)_cppflags)'"
$(package)_config_opts_mingw32 += "QMAKE_CXX = '$($(package)_cxx)'"
$(package)_config_opts_mingw32 += "QMAKE_CXXFLAGS = '$($(package)_cxxflags) $($(package)_cppflags)'"
$(package)_config_opts_mingw32 += "QMAKE_LINK = '$($(package)_cxx)'"
$(package)_config_opts_mingw32 += "QMAKE_LFLAGS = '$($(package)_ldflags)'"
$(package)_config_opts_mingw32 += "QMAKE_LIB = '$($(package)_ar) rc'"
$(package)_config_opts_mingw32 += -device-option CROSS_COMPILE="$(host)-"
$(package)_config_opts_mingw32 += -pch
ifneq ($(LTO),)
$(package)_config_opts_mingw32 += -ltcg
endif

$(package)_config_opts_android = -xplatform android-clang
$(package)_config_opts_android += -android-sdk $(ANDROID_SDK)
$(package)_config_opts_android += -android-ndk $(ANDROID_NDK)
$(package)_config_opts_android += -android-ndk-platform android-$(ANDROID_API_LEVEL)
$(package)_config_opts_android += -egl
$(package)_config_opts_android += -no-dbus
$(package)_config_opts_android += -opengl es2
$(package)_config_opts_android += -qt-freetype
$(package)_config_opts_android += -no-fontconfig
$(package)_config_opts_android += -L $(host_prefix)/lib
$(package)_config_opts_android += -I $(host_prefix)/include
$(package)_config_opts_android += -pch
$(package)_config_opts_android += -no-feature-vulkan

$(package)_config_opts_aarch64_android += -android-arch arm64-v8a
$(package)_config_opts_armv7a_android += -android-arch armeabi-v7a
$(package)_config_opts_x86_64_android += -android-arch x86_64

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qttranslations_file_name),$($(package)_qttranslations_file_name),$($(package)_qttranslations_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qttools_file_name),$($(package)_qttools_file_name),$($(package)_qttools_sha256_hash))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qttranslations_sha256_hash)  $($(package)_source_dir)/$($(package)_qttranslations_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qttools_sha256_hash)  $($(package)_source_dir)/$($(package)_qttools_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir qtbase && \
  tar --no-same-owner --strip-components=1 -xf $($(package)_source) -C qtbase && \
  mkdir qttranslations && \
  tar --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qttranslations_file_name) -C qttranslations && \
  mkdir qttools && \
  tar --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qttools_file_name) -C qttools
endef

define $(package)_preprocess_cmds
  sed -i.old "s|FT_Get_Font_Format|FT_Get_X11_Font_Format|" qtbase/src/platformsupport/fontdatabases/freetype/qfontengine_ft.cpp && \
  sed -i.old "s|updateqm.commands = \$$$$\$$$$LRELEASE|updateqm.commands = $($(package)_extract_dir)/qttools/bin/lrelease|" qttranslations/translations/translations.pro && \
  sed -i.old "/updateqm.depends =/d" qttranslations/translations/translations.pro && \
  sed -i.old "s/src_plugins.depends = src_sql src_network/src_plugins.depends = src_network/" qtbase/src/src.pro && \
  sed -i.old "s|X11/extensions/XIproto.h|X11/X.h|" qtbase/src/plugins/platforms/xcb/qxcbxsettings.cpp && \
  sed -i.old -e 's/if \[ "$$$$XPLATFORM_MAC" = "yes" \]; then xspecvals=$$$$(macSDKify/if \[ "$$$$BUILD_ON_MAC" = "yes" \]; then xspecvals=$$$$(macSDKify/' -e 's|/bin/pwd|pwd|' qtbase/configure && \
  sed -i.old 's/CGEventCreateMouseEvent(0, kCGEventMouseMoved, pos, 0)/CGEventCreateMouseEvent(0, kCGEventMouseMoved, pos, kCGMouseButtonLeft)/' qtbase/src/plugins/platforms/cocoa/qcocoacursor.mm && \
  mkdir -p qtbase/mkspecs/macx-clang-linux &&\
  cp -f qtbase/mkspecs/macx-clang/Info.plist.lib qtbase/mkspecs/macx-clang-linux/ &&\
  cp -f qtbase/mkspecs/macx-clang/Info.plist.app qtbase/mkspecs/macx-clang-linux/ &&\
  cp -f qtbase/mkspecs/macx-clang/qplatformdefs.h qtbase/mkspecs/macx-clang-linux/ &&\
  cp -f $($(package)_patch_dir)/mac-qmake.conf qtbase/mkspecs/macx-clang-linux/qmake.conf && \
  cp -r qtbase/mkspecs/linux-arm-gnueabi-g++ qtbase/mkspecs/bitcoin-linux-g++ && \
  sed -i.old "s/arm-linux-gnueabi-/$(host)-/g" qtbase/mkspecs/bitcoin-linux-g++/qmake.conf && \
  patch -p1 -i $($(package)_patch_dir)/fix_qt_pkgconfig.patch &&\
  patch -p1 -i $($(package)_patch_dir)/fix_configure_mac.patch &&\
  patch -p1 -i $($(package)_patch_dir)/fix_no_printer.patch &&\
  patch -p1 -i $($(package)_patch_dir)/fix_limits_header.patch && \
  patch -p1 -i $($(package)_patch_dir)/fix_mingw_cross_compile.patch && \
  patch -p1 -i $($(package)_patch_dir)/fix_rcc_determinism.patch &&\
  patch -p1 -i $($(package)_patch_dir)/xkb-default.patch &&\
  echo "!host_build: QMAKE_CFLAGS     += $($(package)_cflags) $($(package)_cppflags)" >> qtbase/mkspecs/common/gcc-base.conf && \
  echo "!host_build: QMAKE_CXXFLAGS   += $($(package)_cxxflags) $($(package)_cppflags)" >> qtbase/mkspecs/common/gcc-base.conf && \
  echo "!host_build: QMAKE_LFLAGS     += $($(package)_ldflags)" >> qtbase/mkspecs/common/gcc-base.conf && \
  patch -p1 -i $($(package)_patch_dir)/fix_riscv64_arch.patch &&\
  echo "QMAKE_LINK_OBJECT_MAX = 10" >> qtbase/mkspecs/win32-g++/qmake.conf &&\
  echo "QMAKE_LINK_OBJECT_SCRIPT = object_script" >> qtbase/mkspecs/win32-g++/qmake.conf &&\
  sed -i.old "s|QMAKE_CFLAGS            = |!host_build: QMAKE_CFLAGS            = $($(package)_cflags) $($(package)_cppflags) |" qtbase/mkspecs/win32-g++/qmake.conf && \
  sed -i.old "s|QMAKE_LFLAGS            = |!host_build: QMAKE_LFLAGS            = $($(package)_ldflags) |" qtbase/mkspecs/win32-g++/qmake.conf && \
  sed -i.old "s|QMAKE_CXXFLAGS          = |!host_build: QMAKE_CXXFLAGS            = $($(package)_cxxflags) $($(package)_cppflags) |" qtbase/mkspecs/win32-g++/qmake.conf
endef

define $(package)_config_cmds
  export PKG_CONFIG_SYSROOT_DIR=/ && \
  export PKG_CONFIG_LIBDIR=$(host_prefix)/lib/pkgconfig && \
  export PKG_CONFIG_PATH=$(host_prefix)/share/pkgconfig  && \
  ./configure $($(package)_config_opts) && \
  echo "host_build: QT_CONFIG ~= s/system-zlib/zlib" >> mkspecs/qconfig.pri && \
  echo "CONFIG += force_bootstrap" >> mkspecs/qconfig.pri && \
  $(MAKE) sub-src-clean && \
  cd ../qttranslations && ../qtbase/bin/qmake qttranslations.pro -o Makefile && \
  cd translations && ../../qtbase/bin/qmake translations.pro -o Makefile && cd ../.. && \
  cd qttools/src/linguist/lrelease/ && ../../../../qtbase/bin/qmake lrelease.pro -o Makefile && \
  cd ../lupdate/ && ../../../../qtbase/bin/qmake lupdate.pro -o Makefile && cd ../../../..
endef

define $(package)_build_cmds
  $(MAKE) -C src $(addprefix sub-,$($(package)_qt_libs)) && \
  $(MAKE) -C ../qttools/src/linguist/lrelease && \
  $(MAKE) -C ../qttools/src/linguist/lupdate && \
  $(MAKE) -C ../qttranslations
endef

define $(package)_stage_cmds
  $(MAKE) -C src INSTALL_ROOT=$($(package)_staging_dir) $(addsuffix -install_subtargets,$(addprefix sub-,$($(package)_qt_libs))) && cd .. && \
  $(MAKE) -C qttools/src/linguist/lrelease INSTALL_ROOT=$($(package)_staging_dir) install_target && \
  $(MAKE) -C qttools/src/linguist/lupdate INSTALL_ROOT=$($(package)_staging_dir) install_target && \
  $(MAKE) -C qttranslations INSTALL_ROOT=$($(package)_staging_dir) install_subtargets && \
  if `test -f qtbase/src/plugins/platforms/xcb/xcb-static/libxcb-static.a`; then \
    cp qtbase/src/plugins/platforms/xcb/xcb-static/libxcb-static.a $($(package)_staging_prefix_dir)/lib; \
  fi
endef

define $(package)_postprocess_cmds
  rm -rf native/mkspecs/ native/lib/ lib/cmake/ && \
  rm -f lib/lib*.la lib/*.prl plugins/*/*.prl
endef
