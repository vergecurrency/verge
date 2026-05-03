package=libglvnd
$(package)_version=1.7.0
$(package)_download_path=https://github.com/NVIDIA/libglvnd/archive/refs/tags
$(package)_file_name=v$($(package)_version).tar.gz
$(package)_sha256_hash=073e7292788d4d3eeb45ea6c7bdcce9bfdb3b3eef8d7dbd47f2f30dce046ef98
$(package)_dependencies=libX11 libXext xproto xextproto glproto

define $(package)_set_vars
$(package)_config_opts=--disable-static --enable-shared --with-pic --enable-glx --enable-egl
endef

define $(package)_preprocess_cmds
  NOCONFIGURE=1 ./autogen.sh
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

define $(package)_postprocess_cmds
  rm -rf share
endef
