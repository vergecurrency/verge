package=qt
$(package)_version=6.10.2
$(package)_download_path=https://download.qt.io/official_releases/qt/6.10/$($(package)_version)/submodules
$(package)_suffix=everywhere-src-$($(package)_version).tar.xz
$(package)_file_name=qtbase-$($(package)_suffix)
$(package)_sha256_hash=aeb78d29291a2b5fd53cb55950f8f5065b4978c25fb1d77f627d695ab9adf21e

# Keep dependencies compatible with packages currently present in this repo.
$(package)_darwin_dependencies=openssl native_qt
$(package)_mingw32_dependencies=openssl native_qt
$(package)_linux_dependencies=openssl freetype fontconfig libX11 libxcb libxkbcommon libxcb_util libxcb_util_render libxcb_util_keysyms libxcb_util_image libxcb_util_wm libxcb_util_cursor

$(package)_patches += rcc_hardcode_timestamp.patch
$(package)_patches += root_CMakeLists.txt
$(package)_patches += v4l2.patch
$(package)_patches += windows_func_fix.patch
$(package)_patches += mingw_thread_power_throttling.patch
$(package)_patches += mingw_network_compat.patch
$(package)_patches += libxau-fix.patch
$(package)_patches += toolchain.cmake
$(package)_patches += fix_static_qt_darwin_camera_permissions.patch
#$(package)_patches += fix-static-fontconfig-static-linking.patch

$(package)_qttools_file_name=qttools-$($(package)_suffix)
$(package)_qttools_sha256_hash=1e3d2c07c1fd76d2425c6eaeeaa62ffaff5f79210c4e1a5bc2a6a9db668d5b24

$(package)_qtsvg_file_name=qtsvg-$($(package)_suffix)
$(package)_qtsvg_sha256_hash=f07ff80f38caf235187200345392ca7479445ddf49a36c3694cd52a735dad6e1

$(package)_qtwebsockets_file_name=qtwebsockets-$($(package)_suffix)
$(package)_qtwebsockets_sha256_hash=eccc751bea509ef656d20029693987a0fc03c58e21c38f1351480f3c8eb42ebd

$(package)_qtmultimedia_file_name=qtmultimedia-$($(package)_suffix)
$(package)_qtmultimedia_sha256_hash=93f7ef0106fbd731165a2723f3e436c911fc5e6880f5bc987b55516c20833e2b

$(package)_qtshadertools_file_name=qtshadertools-$($(package)_suffix)
$(package)_qtshadertools_sha256_hash=18d9dbbc4f7e6e96e6ed89a9965dc032e2b58158b65156c035537826216716c9

$(package)_qtwayland_file_name=qtwayland-$($(package)_suffix)
$(package)_qtwayland_sha256_hash=391998eb432719df26a6a67d8efdc67f8bf2afdd76c1ee3381ebff4fe7527ee2

$(package)_extra_sources += $($(package)_qttools_file_name)
$(package)_extra_sources += $($(package)_qtsvg_file_name)
$(package)_extra_sources += $($(package)_qtwebsockets_file_name)
$(package)_extra_sources += $($(package)_qtmultimedia_file_name)
$(package)_extra_sources += $($(package)_qtshadertools_file_name)
$(package)_extra_sources += $($(package)_qtwayland_file_name)

define $(package)_set_vars
$(package)_cmake_system_linux=Linux
$(package)_cmake_system_mingw32=Windows
$(package)_cmake_system_darwin=Darwin

$(package)_config_opts += -DBUILD_SHARED_LIBS=OFF
$(package)_config_opts += -DCMAKE_INSTALL_PREFIX=$(host_prefix)
$(package)_config_opts += -DINSTALL_LIBEXECDIR=$(build_prefix)/bin
$(package)_config_opts += -DQT_BUILD_EXAMPLES=FALSE
$(package)_config_opts += -DQT_BUILD_TESTS=FALSE
$(package)_config_opts += -DQT_GENERATE_SBOM=OFF
$(package)_config_opts += -DQT_FEATURE_cups=OFF
$(package)_config_opts += -DQT_FEATURE_qmake=OFF
$(package)_config_opts += -DQT_FEATURE_egl=OFF
$(package)_config_opts += -DQT_FEATURE_egl_x11=OFF
$(package)_config_opts += -DQT_FEATURE_xcb_egl_plugin=OFF
$(package)_config_opts += -DQT_FEATURE_xcb_glx_plugin=OFF
$(package)_config_opts += -DQT_FEATURE_eglfs=OFF
$(package)_config_opts += -DQT_FEATURE_evdev=OFF
$(package)_config_opts += -DQT_FEATURE_gif=OFF
$(package)_config_opts += -DQT_FEATURE_glib=OFF
$(package)_config_opts += -DQT_FEATURE_icu=OFF
$(package)_config_opts += -DQT_FEATURE_ico=OFF
$(package)_config_opts += -DQT_FEATURE_kms=OFF
$(package)_config_opts += -DQT_FEATURE_linuxfb=OFF
$(package)_config_opts += -DQT_FEATURE_libudev=OFF
$(package)_config_opts += -DQT_FEATURE_mtdev=OFF
$(package)_config_opts += -DQT_FEATURE_openssl=ON
$(package)_config_opts += -DQT_FEATURE_openssl_linked=ON
$(package)_config_opts += -DQT_FEATURE_openvg=OFF
$(package)_config_opts += -DQT_FEATURE_permissions=ON
$(package)_config_opts += -DQT_FEATURE_reduce_relocations=OFF
$(package)_config_opts += -DQT_FEATURE_schannel=OFF
$(package)_config_opts += -DQT_FEATURE_sctp=OFF
$(package)_config_opts += -DQT_FEATURE_securetransport=OFF
$(package)_config_opts += -DQT_FEATURE_system_proxies=OFF
$(package)_config_opts += -DQT_FEATURE_use_gold_linker_alias=OFF
$(package)_config_opts += -DQT_FEATURE_zstd=OFF
$(package)_config_opts += -DQT_FEATURE_pkg_config=ON
$(package)_config_opts += -DQT_FEATURE_system_png=OFF
$(package)_config_opts += -DQT_FEATURE_system_pcre2=OFF
$(package)_config_opts += -DQT_FEATURE_system_harfbuzz=OFF
$(package)_config_opts += -DQT_FEATURE_system_zlib=OFF
$(package)_config_opts += -DQT_FEATURE_colordialog=OFF
$(package)_config_opts += -DQT_FEATURE_dial=OFF
$(package)_config_opts += -DQT_FEATURE_fontcombobox=OFF
$(package)_config_opts += -DQT_FEATURE_image_heuristic_mask=OFF
$(package)_config_opts += -DQT_FEATURE_keysequenceedit=OFF
$(package)_config_opts += -DQT_FEATURE_lcdnumber=OFF
$(package)_config_opts += -DQT_FEATURE_networkdiskcache=OFF
$(package)_config_opts += -DQT_FEATURE_pdf=OFF
$(package)_config_opts += -DQT_FEATURE_printdialog=OFF
$(package)_config_opts += -DQT_FEATURE_printer=OFF
$(package)_config_opts += -DQT_FEATURE_printpreviewdialog=OFF
$(package)_config_opts += -DQT_FEATURE_printpreviewwidget=OFF
$(package)_config_opts += -DQT_FEATURE_printsupport=OFF
$(package)_config_opts += -DQT_FEATURE_sessionmanager=OFF
$(package)_config_opts += -DQT_FEATURE_spatialaudio=OFF
$(package)_config_opts += -DQT_FEATURE_sql=OFF
$(package)_config_opts += -DQT_FEATURE_syntaxhighlighter=OFF
$(package)_config_opts += -DQT_FEATURE_tabletevent=OFF
$(package)_config_opts += -DQT_FEATURE_textmarkdownwriter=OFF
$(package)_config_opts += -DQT_FEATURE_textodfwriter=OFF
$(package)_config_opts += -DQT_FEATURE_topleveldomain=OFF
$(package)_config_opts += -DQT_FEATURE_undocommand=OFF
$(package)_config_opts += -DQT_FEATURE_undogroup=OFF
$(package)_config_opts += -DQT_FEATURE_undostack=OFF
$(package)_config_opts += -DQT_FEATURE_undoview=OFF
$(package)_config_opts += -DQT_FEATURE_vnc=OFF

$(package)_config_opts_linux += -DQT_QMAKE_TARGET_MKSPEC=linux-g++
$(package)_config_opts_linux += -DQT_FEATURE_xcb=ON
$(package)_config_opts_linux += -DQT_FEATURE_xcb_xlib=ON
$(package)_config_opts_linux += -DQT_FEATURE_xlib=ON
$(package)_config_opts_linux += -DQT_FEATURE_xkbcommon=ON
$(package)_config_opts_linux += -DQT_FEATURE_xkbcommon_x11=ON
$(package)_config_opts_linux += -DQT_FEATURE_freetype=ON
$(package)_config_opts_linux += -DQT_FEATURE_system_freetype=ON
$(package)_config_opts_linux += -DQT_FEATURE_fontconfig=ON
$(package)_config_opts_linux += -DINPUT_opengl=no
$(package)_config_opts_linux += -DQT_FEATURE_opengl=OFF
$(package)_config_opts_linux += -DQT_FEATURE_opengles2=OFF
$(package)_config_opts_linux += -DQT_FEATURE_opengles3=OFF
$(package)_config_opts_linux += -DQT_FEATURE_opengles31=OFF
$(package)_config_opts_linux += -DQT_FEATURE_opengles32=OFF
$(package)_config_opts_linux += -DQT_FEATURE_opengl_desktop=OFF
$(package)_config_opts_linux += -DQT_FEATURE_vulkan=OFF
$(package)_config_opts_linux += -DQT_FEATURE_backtrace=OFF
$(package)_config_opts_linux += -DQT_FEATURE_dbus=OFF
$(package)_config_opts_linux += -DQT_FEATURE_dbus_linked=OFF
$(package)_config_opts_linux += -DQT_FEATURE_wayland_client=OFF
$(package)_config_opts_linux += -DQT_FEATURE_wayland_server=OFF
$(package)_config_opts_linux += -DBUILD_WITH_PCH=OFF

$(package)_config_opts_mingw32 += -DQT_QMAKE_TARGET_MKSPEC=win32-g++
$(package)_config_opts_mingw32 += -DQT_HOST_PATH=$(build_prefix)/qt-host
$(package)_config_opts_mingw32 += -DQT_HOST_PATH_CMAKE_DIR=$(build_prefix)/qt-host/lib/cmake
$(package)_config_opts_mingw32 += -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake
$(package)_config_opts_mingw32 += -DINPUT_opengl=no
$(package)_config_opts_mingw32 += -DQT_FEATURE_dbus=OFF
$(package)_config_opts_mingw32 += -DQT_FEATURE_freetype=OFF
$(package)_config_opts_mingw32 += -DQT_FEATURE_ffmpeg=OFF
$(package)_config_opts_mingw32 += -DQT_FEATURE_wmf=ON
$(package)_config_opts_mingw32 += -DFEATURE_intelcet=OFF
$(package)_config_opts_mingw32 += -DBUILD_WITH_PCH=ON

$(package)_config_opts_darwin += -DQT_QMAKE_TARGET_MKSPEC=macx-clang
$(package)_config_opts_darwin += -DQT_HOST_PATH=$(build_prefix)/qt-host
$(package)_config_opts_darwin += -DQT_HOST_PATH_CMAKE_DIR=$(build_prefix)/qt-host/lib/cmake
$(package)_config_opts_darwin += -DCMAKE_TOOLCHAIN_FILE=toolchain.cmake
$(package)_config_opts_darwin += -DQT_FEATURE_accessibility=OFF
$(package)_config_opts_darwin += -DQT_FEATURE_dbus=OFF
$(package)_config_opts_darwin += -DQT_FEATURE_freetype=OFF
$(package)_config_opts_darwin += -DQT_FEATURE_ffmpeg=OFF
$(package)_config_opts_darwin += -DBUILD_WITH_PCH=OFF
$(package)_config_opts_darwin += '-DQT_QMAKE_DEVICE_OPTIONS=MAC_SDK_PATH=$(host_prefix)/native/SDK;MAC_SDK_VERSION=$(OSX_SDK_VERSION);CROSS_COMPILE=$(host)-;MAC_TARGET=$(host);XCODE_VERSION=$(XCODE_VERSION)'
$(package)_config_opts_darwin += -DQT_NO_APPLE_SDK_AND_XCODE_CHECK=ON
$(package)_config_opts_darwin += -DQT_FEATURE_appstore_compliant=ON

$(package)_config_opts += -G Ninja

$(package)_openssl_flags_$(host_os)="-lssl -lcrypto -lpthread -ldl"
$(package)_openssl_flags_mingw32="-lssl -lcrypto -lws2_32"

endef

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qttools_file_name),$($(package)_qttools_file_name),$($(package)_qttools_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qtsvg_file_name),$($(package)_qtsvg_file_name),$($(package)_qtsvg_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qtwebsockets_file_name),$($(package)_qtwebsockets_file_name),$($(package)_qtwebsockets_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qtmultimedia_file_name),$($(package)_qtmultimedia_file_name),$($(package)_qtmultimedia_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qtshadertools_file_name),$($(package)_qtshadertools_file_name),$($(package)_qtshadertools_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qtwayland_file_name),$($(package)_qtwayland_file_name),$($(package)_qtwayland_sha256_hash))
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qttools_sha256_hash)  $($(package)_source_dir)/$($(package)_qttools_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qtsvg_sha256_hash)  $($(package)_source_dir)/$($(package)_qtsvg_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qtwebsockets_sha256_hash)  $($(package)_source_dir)/$($(package)_qtwebsockets_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qtmultimedia_sha256_hash)  $($(package)_source_dir)/$($(package)_qtmultimedia_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qtshadertools_sha256_hash)  $($(package)_source_dir)/$($(package)_qtshadertools_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qtwayland_sha256_hash)  $($(package)_source_dir)/$($(package)_qtwayland_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir qtbase && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source) -C qtbase && \
  mkdir qttools && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qttools_file_name) -C qttools && \
  mkdir qtsvg && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qtsvg_file_name) -C qtsvg && \
  mkdir qtwebsockets && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qtwebsockets_file_name) -C qtwebsockets && \
  mkdir qtmultimedia && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qtmultimedia_file_name) -C qtmultimedia && \
  mkdir qtshadertools && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qtshadertools_file_name) -C qtshadertools && \
  mkdir qtwayland && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qtwayland_file_name) -C qtwayland
endef

define $(package)_preprocess_cmds
  cp $($(package)_patch_dir)/root_CMakeLists.txt CMakeLists.txt && \
  patch -p1 -i $($(package)_patch_dir)/rcc_hardcode_timestamp.patch && \
  patch -p1 -i $($(package)_patch_dir)/windows_func_fix.patch && \
  cp $($(package)_patch_dir)/toolchain.cmake . && \
  patch -p1 -i $($(package)_patch_dir)/mingw_thread_power_throttling.patch && \
  patch -p1 -i $($(package)_patch_dir)/mingw_network_compat.patch && \
  sed -i -e 's|@cmake_system_name@|$($(package)_cmake_system_$(host_os))|' \
         -e 's|@target@|$(host)|' \
         -e 's|@host_prefix@|$(host_prefix)|' \
         -e 's|@cmake_c_flags@|$(darwin_CC)|' \
         -e 's|@cmake_cxx_flags@|$(darwin_CXX)|' \
         -e 's|@cmake_ld_flags@|$(darwin_LDFLAGS)|' \
         -e 's|@wmf_libs@|$(WMF_LIBS)|' \
      toolchain.cmake && \
  cd qtbase && \
  sed -i -e 's/qt_find_package(XKB 0.9.0 /qt_find_package(XKB 0.8.4 /' \
         -e 's/qt_find_package(XKB_COMMON_X11 0.9.0 /qt_find_package(XKB_COMMON_X11 0.8.4 /' src/gui/configure.cmake && \
  patch -p1 -i $($(package)_patch_dir)/libxau-fix.patch && \
  patch -p1 -i $($(package)_patch_dir)/fix_static_qt_darwin_camera_permissions.patch && \
  cd ../qtmultimedia && \
  patch -p1 -i $($(package)_patch_dir)/v4l2.patch
endef

define $(package)_config_cmds
  mkdir -p $(build_prefix)/qt-host $(build_prefix)/qt-host/lib/cmake && \
  export OPENSSL_LIBS=${$(package)_openssl_flags_$(host_os)} && \
  export PKG_CONFIG_SYSROOT_DIR=/ && \
  export PKG_CONFIG_LIBDIR=$(host_prefix)/lib/pkgconfig && \
  export QT_MAC_SDK_NO_VERSION_CHECK=1 && \
  cmake -S . -B . $($(package)_config_opts)
endef

define $(package)_build_cmds
  export LD_LIBRARY_PATH="$(build_prefix)/lib/:$(QT_LIBS_LIBS)" && \
  env -u C_INCLUDE_PATH -u CPLUS_INCLUDE_PATH -u OBJC_INCLUDE_PATH -u OBJCPLUS_INCLUDE_PATH -u CPATH -u LIBRARY_PATH cmake --build . --parallel
endef

define $(package)_stage_cmds
  DESTDIR=$($(package)_staging_dir) cmake --install . && \
  mkdir -p $($(package)_staging_dir)$(host_prefix)/plugins/platforms && \
  if test -f qtbase/plugins/platforms/libqwindows.a; then \
    cp qtbase/plugins/platforms/libqwindows.a $($(package)_staging_dir)$(host_prefix)/plugins/platforms/; \
  fi && \
  if test -f qtbase/plugins/platforms/qwindows.prl; then \
    cp qtbase/plugins/platforms/qwindows.prl $($(package)_staging_dir)$(host_prefix)/plugins/platforms/; \
  fi && \
  if test -f qtbase/plugins/platforms/libqdirect2d.a; then \
    cp qtbase/plugins/platforms/libqdirect2d.a $($(package)_staging_dir)$(host_prefix)/plugins/platforms/; \
  fi && \
  if test -f qtbase/plugins/platforms/qdirect2d.prl; then \
    cp qtbase/plugins/platforms/qdirect2d.prl $($(package)_staging_dir)$(host_prefix)/plugins/platforms/; \
  fi
endef

define $(package)_postprocess_cmds
  rm -f lib/lib*.la
endef
