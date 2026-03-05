package=libXau
$(package)_version=1.0.11
$(package)_download_path=https://www.x.org/releases/individual/lib/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=3a321aaceb803577a4776a5efe78836eb095a9e44bbc7a465d29463e1a14f189
$(package)_dependencies=xproto

# When updating this package, check the default value of
# --disable-xthreads. It is currently enabled.
define $(package)_set_vars
  $(package)_config_opts=--disable-shared --disable-lint-library --without-lint
  $(package)_config_opts += --disable-dependency-tracking --enable-option-checking
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

define $(package)_postprocess_cmds
  rm -rf share lib/*.la
endef
