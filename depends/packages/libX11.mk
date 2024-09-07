package=libX11
$(package)_version=1.8.10
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=B7A1A90D881BB7B94DF5CF31509E6B03F15C0972D3AC25AB0441F5FBC789650F
$(package)_dependencies=libxcb xtrans xextproto xorgproto

define $(package)_set_vars
$(package)_config_opts_arm_linux_gnueabihf += --host=arm-linux-gnueabihf --build=x86_64-linux-gnu 
$(package)_config_opts_arm_linux_gnueabihf += --disable-dependency-tracking --without-xsltproc
$(package)_config_opts +=--disable-xkb --disable-static
$(package)_config_opts_linux=--with-pic
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