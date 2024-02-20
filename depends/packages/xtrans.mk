package=xtrans
$(package)_version=1.4.0
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=377c4491593c417946efcd2c7600d1e62639f7a8bbca391887e2c4679807d773
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