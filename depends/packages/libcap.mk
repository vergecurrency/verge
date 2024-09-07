package=libcap
$(package)_version=2.70
$(package)_download_path=https://mirrors.edge.kernel.org/pub/linux/libs/security/linux-privs/libcap2/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=D3B777ED413C9FAFFACE03B917E171854709B5E4BE38DBFB9219AAF7DFD4EEA6

define $(package)_set_vars
  $(package)_config_opts=--disable-static
endef

define $(package)_build_cmds
  $(MAKE) DESTDIR=`pwd`/out prefix=/ RAISE_SETFCAP=no lib="lib" install && \
  mkdir -p $($(package)_staging_dir)$(host_prefix)/bin && \
  cp -a out/sbin/* $($(package)_staging_dir)$(host_prefix)/bin/ && \
  \
  mkdir -p $($(package)_staging_dir)$(host_prefix)/lib && \
  cp -a out/lib/* $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp -a out/lib/pkgconfig/* $($(package)_staging_dir)$(host_prefix)/lib/pkgconfig/
endef
