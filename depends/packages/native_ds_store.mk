package=native_ds_store
$(package)_version=1.3.1
$(package)_download_path=https://github.com/dmgbuild/ds_store/archive/refs/tags/
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=4B09F5EB6BA13C4F2350EB8761726E88E30AA329BF790DE7AE513E465A681E57
$(package)_install_libdir=$(build_prefix)/lib/python3/dist-packages

define $(package)_build_cmds
    python setup.py build
endef

define $(package)_stage_cmds
    mkdir -p $($(package)_install_libdir) && \
    python setup.py install --root=$($(package)_staging_dir) --prefix=$(build_prefix) --install-lib=$($(package)_install_libdir)
endef
