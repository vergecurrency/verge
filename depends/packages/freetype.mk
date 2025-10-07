package=freetype
$(package)_version=2.14.1
$(package)_download_path=https://sourceforge.net/projects/freetype/files/freetype2/$($(package)_version)/
$(package)_file_name=freetype-$($(package)_version).tar.xz
$(package)_sha256_hash=32427E8C471AC095853212A37AEF816C60B42052D4D9E48230BAB3BDF2936CCC

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
