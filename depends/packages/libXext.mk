package=libXext
$(package)_version=1.3.7
$(package)_download_path=http://xorg.freedesktop.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=6564608DC3B816B0CFDDF0C7DDC62BC579055DD70B2F28113A618DF2ACB64189
$(package)_dependencies=xorgproto xextproto libX11 libXau

define $(package)_set_vars
  $(package)_config_opts=--disable-static
  $(package)_config_opts_arm_linux_gnueabihf += --host=arm-linux-gnueabihf --build=x86_64-linux-gnu
  $(package)_config_opts_arm_linux_gnueabihf += --disable-dependency-tracking --without-xsltproc
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
