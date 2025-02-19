package=libseccomp
$(package)_version=2.6.0
$(package)_download_path=https://github.com/seccomp/libseccomp/releases/download/v$($(package)_version)/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=83b6085232d1588c379dc9b9cae47bb37407cf262e6e74993c61ba72d2a784dc

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
