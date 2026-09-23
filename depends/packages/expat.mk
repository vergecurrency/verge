package=expat
$(package)_version=2.8.3
$(package)_download_path=https://github.com/libexpat/libexpat/releases/download/R_$(subst .,_,$($(package)_version))/
$(package)_file_name=$(package)-$($(package)_version).tar.xz
$(package)_sha256_hash=f6256df90c906773d344da084402b7d3e4f22ed41b1a59c989098a83d3ea0c85

# -D_DEFAULT_SOURCE defines __USE_MISC, which exposes additional
# definitions in endian.h, which are required for a working
# endianess check in configure when building with -flto.
define $(package)_set_vars
  $(package)_config_opts=--disable-shared --without-docbook --without-tests --without-examples
  $(package)_config_opts += --disable-dependency-tracking --enable-option-checking
  $(package)_config_opts += --without-xmlwf
  $(package)_config_opts_linux=--with-pic
  $(package)_cppflags += -D_DEFAULT_SOURCE
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

define $(package)_postprocess_cmds
  rm -rf share lib/cmake lib/*.la
endef
