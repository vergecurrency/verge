package=libXext
$(package)_version=1.3.4
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=59ad6fcce98deaecc14d39a672cf218ca37aba617c9a0f691cac3bcd28edf82b
$(package)_dependencies=xorgproto xextproto libX11 libXau
$(package)_patches=patch-malloc-zero-check.patch

define $(package)_set_vars
  $(package)_config_opts=--disable-static
  $(package)_config_opts_arm_linux_gnueabihf += --host=arm-linux-gnueabihf --build=x86_64-linux-gnu
  $(package)_config_opts_arm_linux_gnueabihf += --disable-dependency-tracking --without-xsltproc
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
