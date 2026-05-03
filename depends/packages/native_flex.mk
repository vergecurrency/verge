package=native_flex
$(package)_version=2.6.4
$(package)_download_path=https://github.com/westes/flex/releases/download/v$($(package)_version)
$(package)_file_name=flex-$($(package)_version).tar.gz
$(package)_sha256_hash=e87aae032bf07c26f85ac0ed3250998c37621d95f8bd748b31f15b33c45ee995

define $(package)_set_vars
$(package)_config_opts=--disable-shared --enable-static
endef

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-strip
endef

define $(package)_postprocess_cmds
  rm -rf include lib share
endef
