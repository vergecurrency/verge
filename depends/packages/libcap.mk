package=libcap
$(package)_version=2.25
$(package)_download_path=https://git.kernel.org/pub/scm/libs/libcap/libcap.git/snapshot/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=be671ebdab7b5203be19cc76f2b4e29cbd962cfc4cf0030519fc39aee6db6d3d

define $(package)_set_vars
  $(package)_config_opts=--disable-static
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
