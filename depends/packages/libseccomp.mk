package=libseccomp
$(package)_version=2.5.5
$(package)_download_path=https://github.com/seccomp/libseccomp/releases/download/v$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=248a2c8a4d9b9858aa6baf52712c34afefcf9c9e94b76dce02c1c9aa25fb3375

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