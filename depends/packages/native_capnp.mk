package=native_capnp
$(package)_version=1.2.0
$(package)_download_path=https://capnproto.org/
$(package)_download_file=capnproto-$($(package)_version).tar.gz
$(package)_file_name=capnproto-cxx-$($(package)_version).tar.gz
$(package)_sha256_hash=ED00E44ECBBDA5186BC78A41BA64A8DC4A861B5F8D4E822959B0144AE6FD42EF

define $(package)_set_vars
  $(package)_config_opts := -DBUILD_TESTING=OFF
  $(package)_config_opts += -DWITH_OPENSSL=OFF
  $(package)_config_opts += -DWITH_ZLIB=OFF
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
