package=libX11
$(package)_version=1.8.12
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=220FBCF54B6E4D8DC40076FF4AB87954358019982490B33C7802190B62D89CE1
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
