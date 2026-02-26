package=libX11
$(package)_version=1.8.10
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=b7a1a90d881bb7b94df5cf31509e6b03f15c0972d3ac25ab0441f5fbc789650f
$(package)_dependencies=libxcb xtrans xextproto xproto

define $(package)_set_vars
$(package)_config_opts=--disable-xkb --disable-static
$(package)_config_opts_linux=--with-pic
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
