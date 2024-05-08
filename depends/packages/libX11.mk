package=libX11
$(package)_version=1.6.9
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=9cc7e8d000d6193fa5af580d50d689380b8287052270f5bb26a5fb6b58b2bed1
$(package)_dependencies=libxcb xtrans xextproto xorgproto
$(package)_patches=patch-malloc-zero-check.patch

define $(package)_set_vars
$(package)_config_opts_arm_linux_gnueabihf += --host=arm-linux-gnueabihf --build=x86_64-linux-gnu 
$(package)_config_opts_arm_linux_gnueabihf += --disable-dependency-tracking --without-xsltproc
$(package)_config_opts +=--disable-xkb --disable-static
$(package)_config_opts_linux=--with-pic
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/patch-malloc-zero-check.patch &&\
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