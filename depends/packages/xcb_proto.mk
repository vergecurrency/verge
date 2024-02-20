package=xcb_proto
$(package)_version=1.16.0
$(package)_download_path=https://xorg.freedesktop.org/archive/individual/proto
$(package)_file_name=xcb-proto-$($(package)_version).tar.xz
$(package)_sha256_hash=a75a1848ad2a89a82d841a51be56ce988ff3c63a8d6bf4383ae3219d8d915119

define $(package)_config_cmds
  $($(package)_autoconf)
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
endef

define $(package)_postprocess_cmds
  rm -rf lib/python*/site-packages/xcbgen/__pycache__
endef