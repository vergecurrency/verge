package=native_capnp
$(package)_version=1.3.0
$(package)_download_path=https://capnproto.org/
$(package)_download_file=capnproto-$($(package)_version).tar.gz
$(package)_file_name=capnproto-cxx-$($(package)_version).tar.gz
$(package)_sha256_hash=01AB2BA7F52FCC3C51A10E22935AAE56F3BC5E99B726B7E507FE6700CB12147D

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
