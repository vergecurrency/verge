package=libcap
$(package)_version=2.26
$(package)_download_path=https://git.kernel.org/pub/scm/libs/libcap/libcap.git/snapshot/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=94fa5a39f1a67a2c41bc01a895702cb3f46caef37dae7a6c8cceb1f5853afdcd

define $(package)_set_vars
  $(package)_config_opts=--disable-static
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
