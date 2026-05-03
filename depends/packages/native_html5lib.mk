package=native_html5lib
$(package)_version=1.1
$(package)_download_path=https://files.pythonhosted.org/packages/source/h/html5lib
$(package)_file_name=html5lib-$($(package)_version).tar.gz
$(package)_sha256_hash=b2e5b40261e20f354d198eae92afc10d750afb487ed5e50f9c4eaf07c184146f
$(package)_dependencies=native_webencodings

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
