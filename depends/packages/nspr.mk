package=nspr
$(package)_version=4.36
$(package)_download_path=https://archive.mozilla.org/pub/nspr/releases/v$($(package)_version)/src
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=55dec317f1401cd2e5dba844d340b930ab7547f818179a4002bce62e6f1c6895

define $(package)_set_vars
$(package)_config_opts=--disable-debug --enable-optimize
$(package)_config_opts_x86_64_linux=--enable-64bit
endef

define $(package)_config_cmds
  cd nspr && $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE) -C nspr
endef

define $(package)_stage_cmds
  $(MAKE) -C nspr DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf bin
endef
