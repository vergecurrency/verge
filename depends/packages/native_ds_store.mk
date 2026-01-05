package=native_ds_store
$(package)_version=1.3.2
$(package)_download_path=https://github.com/dmgbuild/ds_store/archive/refs/tags/
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=48BE8EBB432889EBED4877762779D87126AFF5CEF9E57D7174E1CDCDCD120C17
$(package)_install_libdir=$(build_prefix)/lib/python3/dist-packages

define $(package)_build_cmds
    python setup.py build
endef

define $(package)_stage_cmds
    mkdir -p $($(package)_install_libdir) && \
    python setup.py install --root=$($(package)_staging_dir) --prefix=$(build_prefix) --install-lib=$($(package)_install_libdir)
endef
