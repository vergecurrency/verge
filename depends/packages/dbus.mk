package=dbus
$(package)_version=1.15.10
$(package)_download_path=https://dbus.freedesktop.org/releases/dbus
$(package)_file_name=dbus-$($(package)_version).tar.xz
$(package)_sha256_hash=F700F2F1D0473F11E52F3F3E179F577F31B85419F9AE1972AF8C3DB0BCFDE178
$(package)_dependencies=expat
$(package)_patches=remove-DDBUS_STATIC_BUILD.patch

define $(package)_set_vars
  $(package)_config_opts=--disable-shared --disable-maintainer-mode --disable-xml-docs --disable-doxygen-docs
  $(package)_config_opts+=--disable-ducktype-docs
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/remove-DDBUS_STATIC_BUILD.patch
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm lib/*.la
endef