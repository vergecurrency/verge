package=xorgproto
$(package)_version=2023.2
$(package)_download_path=https://xorg.freedesktop.org/archive/individual/proto
$(package)_file_name=xorgproto-$($(package)_version).tar.xz
$(package)_sha256_hash=b61fbc7db82b14ce2dc705ab590efc32b9ad800037113d1973811781d5118c2c

define $(package)_set_vars
$(package)_config_opts=--without-fop --without-xmlto --without-xsltproc --disable-specs
$(package)_config_opts += --disable-dependency-tracking --enable-option-checking
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