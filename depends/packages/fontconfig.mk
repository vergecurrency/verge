package=fontconfig
$(package)_version=2.16.0
$(package)_download_path=http://www.freedesktop.org/software/fontconfig/release/
$(package)_file_name=$(package)-$($(package)_version).tar.xz
$(package)_sha256_hash=6A33DC555CC9BA8B10CAF7695878EF134EEB36D0AF366041F639B1DA9B6ED220
$(package)_dependencies=freetype expat

define $(package)_set_vars
  $(package)_config_opts=--disable-docs --disable-static --disable-libxml2 --disable-iconv
  $(package)_config_opts += --disable-dependency-tracking --enable-option-checking
  $(package)_cflags += -Wno-implicit-function-declaration
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
  rm -rf var lib/*.la
endef
