package=libX11
$(package)_version=1.8.9
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=57CA5F07D263788AD661A86F4139412E8B699662E6B60C20F1F028C25A935E48
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