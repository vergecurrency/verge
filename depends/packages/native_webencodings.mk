package=native_webencodings
$(package)_version=0.5.1
$(package)_download_path=https://files.pythonhosted.org/packages/source/w/webencodings
$(package)_file_name=webencodings-$($(package)_version).tar.gz
$(package)_sha256_hash=b36a1c245f2d304965eb4e0a82848379241dc04b865afcc4aab16748587e1923

define $(package)_config_cmds
  true
endef

define $(package)_build_cmds
  true
endef

define $(package)_stage_cmds
  python3 setup.py install --prefix=$(build_prefix) --root=$($(package)_staging_dir)
endef

define $(package)_postprocess_cmds
  rm -rf bin
endef
