package=libseccomp
$(package)_version=2.3.3
$(package)_download_path=https://github.com/seccomp/libseccomp/releases/download/v$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=7fc28f4294cc72e61c529bedf97e705c3acf9c479a8f1a3028d4cd2ca9f3b155

ifneq ($(host_os),mingw32)
ifneq ($(host_os),darwin)

define $(package)_set_vars
  $(package)_config_opts=--disable-static
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

endif
endif