package=util-macros
GCCFLAGS?=
$(package)_version=1.19.3
$(package)_download_path=http://www.x.org/releases/individual/util/
$(package)_file_name=$(package)-$($(package)_version).tar.bz2
$(package)_sha256_hash=0f812e6e9d2786ba8f54b960ee563c0663ddbe2434bf24ff193f5feab1f31971
$(package)_dependencies=

define $(package)_set_vars
$(package)_config_opts=--disable-shared --enable-static --disable-devel-docs --without-doxygen --without-launchd
$(package)_config_opts += --disable-dependency-tracking --enable-option-checking
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

#define $(package)_postprocess_cmds
#  rm -rf share lib/*.la
#endef
