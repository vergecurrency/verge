package=freetype
$(package)_version=2.11.0
$(package)_download_path=https://download.savannah.gnu.org/releases/freetype
$(package)_file_name=freetype-$($(package)_version).tar.xz
$(package)_sha256_hash=8bee39bd3968c4804b70614a0a3ad597299ad0e824bc8aad5ce8aaf48067bde7

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
