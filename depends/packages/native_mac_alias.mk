package=native_mac_alias
$(package)_version=2.2.2
$(package)_download_path=https://github.com/al45tair/mac_alias/archive/
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=57B1BFFF3F942F089E1BBCDA2A8CC5F300744D84DAD9E624DF832BF55CDB6218
$(package)_install_libdir=$(build_prefix)/lib/python/dist-packages

define $(package)_build_cmds
    python setup.py build
endef

define $(package)_stage_cmds
    mkdir -p $($(package)_install_libdir) && \
    python setup.py install --root=$($(package)_staging_dir) --prefix=$(build_prefix) --install-lib=$($(package)_install_libdir)
endef
