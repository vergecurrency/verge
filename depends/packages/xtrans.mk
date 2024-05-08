package=xtrans
$(package)_version=1.5.0
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=A806F8A92F879DCD0146F3F1153FDFFE845F2FC0DF9B1A26C19312B7B0A29C86
$(package)_dependencies=

define $(package)_set_vars
$(package)_config_opts_linux=--with-pic --disable-static
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