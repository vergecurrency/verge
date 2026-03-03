package=libxcb_util_cursor
$(package)_version=0.1.5
$(package)_download_path=https://xcb.freedesktop.org/dist
$(package)_file_name=xcb-util-cursor-$($(package)_version).tar.xz
$(package)_sha256_hash=0caf99b0d60970f81ce41c7ba694e5eaaf833227bb2cbcdb2f6dc9666a663c57
$(package)_dependencies=libxcb libxcb_util libxcb_util_image libxcb_util_render

define $(package)_set_vars
$(package)_config_opts=--disable-static --disable-devel-docs --without-doxygen
$(package)_config_opts += --disable-dependency-tracking --enable-option-checking
endef

define $(package)_preprocess_cmds
  cp -f $(BASEDIR)/config.guess $(BASEDIR)/config.sub .
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
  rm -rf share/man share/doc lib/*.la
endef
