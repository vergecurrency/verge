package=libcap
$(package)_version=2.24
$(package)_download_path=https://mirrors.edge.kernel.org/pub/linux/libs/security/linux-privs/libcap2/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=3a9640e0712d545109cecef83c2614c68951e5fcf0fff4dac4df1a893c52bdbb

define $(package)_set_vars
  $(package)_config_opts=--disable-static
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef
