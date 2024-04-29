package=freetype
$(package)_version=2.13.2
$(package)_download_path=https://sourceforge.net/projects/freetype/files/freetype2/$($(package)_version)/
$(package)_file_name=freetype-$($(package)_version).tar.xz
$(package)_sha256_hash=12991c4e55c506dd7f9b765933e62fd2be2e06d421505d7950a132e4f1bb484d

define $(package)_set_vars
  $(package)_config_opts  = --without-zlib --without-png --without-harfbuzz --without-bzip2 --enable-static --disable-shared
  $(package)_config_opts += --enable-option-checking --without-brotli
  $(package)_config_opts += --with-pic
endef

define $(package)_config_cmds
  printenv && \
  echo "$($(package)_autoconf)" && \
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf share/man lib/*.la
endef
