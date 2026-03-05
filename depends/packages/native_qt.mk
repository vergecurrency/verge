package=native_qt
$(package)_version=6.10.2
$(package)_download_path=https://download.qt.io/official_releases/qt/6.10/$($(package)_version)/submodules
$(package)_file_name=qtbase-everywhere-src-$($(package)_version).tar.xz
$(package)_sha256_hash=aeb78d29291a2b5fd53cb55950f8f5065b4978c25fb1d77f627d695ab9adf21e
$(package)_qtshadertools_file_name=qtshadertools-everywhere-src-$($(package)_version).tar.xz
$(package)_qtshadertools_sha256_hash=18d9dbbc4f7e6e96e6ed89a9965dc032e2b58158b65156c035537826216716c9
$(package)_extra_sources += $($(package)_qtshadertools_file_name)

define $(package)_set_vars
$(package)_config_opts += -DBUILD_SHARED_LIBS=OFF
$(package)_config_opts += -DCMAKE_INSTALL_PREFIX=$(build_prefix)/qt-host
$(package)_config_opts += -DINSTALL_LIBEXECDIR=$(build_prefix)/qt-host/bin
$(package)_config_opts += -DQT_BUILD_EXAMPLES=FALSE
$(package)_config_opts += -DQT_BUILD_TESTS=FALSE
$(package)_config_opts += -DQT_GENERATE_SBOM=OFF
$(package)_config_opts += -DQT_FEATURE_dbus=OFF
$(package)_config_opts += -DQT_FEATURE_openssl=OFF
$(package)_config_opts += -DQT_FEATURE_printsupport=OFF
$(package)_config_opts += -DINPUT_opengl=no
$(package)_config_opts += -DQT_FEATURE_opengl=OFF
$(package)_config_opts += -DQT_FEATURE_opengles2=OFF
$(package)_config_opts += -DQT_FEATURE_opengl_desktop=OFF
$(package)_config_opts += -G Ninja
endef

define $(package)_extract_cmds
  mkdir -p $($(package)_extract_dir) && \
  echo "$($(package)_sha256_hash)  $($(package)_source)" > $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  echo "$($(package)_qtshadertools_sha256_hash)  $($(package)_source_dir)/$($(package)_qtshadertools_file_name)" >> $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  $(build_SHA256SUM) -c $($(package)_extract_dir)/.$($(package)_file_name).hash && \
  mkdir qtbase && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source) -C qtbase && \
  mkdir qtshadertools && \
  $(build_TAR) --no-same-owner --strip-components=1 -xf $($(package)_source_dir)/$($(package)_qtshadertools_file_name) -C qtshadertools
endef

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash)) && \
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_qtshadertools_file_name),$($(package)_qtshadertools_file_name),$($(package)_qtshadertools_sha256_hash))
endef

define $(package)_config_cmds
  cp $(PATCHES_PATH)/qt/root_CMakeLists.txt CMakeLists.txt && \
  sed -i 's|qtbase;qtsvg;qtwebsockets;qtshadertools;qtmultimedia;qtwayland|qtbase;qtshadertools|' CMakeLists.txt && \
  cmake -S . -B . $($(package)_config_opts)
endef

define $(package)_build_cmds
  cmake --build . --parallel
endef

define $(package)_stage_cmds
  DESTDIR=$($(package)_staging_dir) cmake --install .
endef
