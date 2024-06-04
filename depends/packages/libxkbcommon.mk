package=libxkbcommon
$(package)_version=1.7.0
$(package)_download_path=https://xkbcommon.org/download/
$(package)_file_name=libxkbcommon-$($(package)_version).tar.xz
$(package)_sha256_hash=65782f0a10a4b455af9c6baab7040e2f537520caa2ec2092805cdfd36863b247
$(package)_dependencies=libxcb
$(package)_patches=no-test-x11.patch toolchain.txt

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/no-test-x11.patch && \
  cp $($(package)_patch_dir)/toolchain.txt toolchain.txt && \
  sed -i -e 's|@host_prefix@|$(host_prefix)|' \
         -e 's|@cc@|$($(package)_cc)|' \
         -e 's|@cxx@|$($(package)_cxx)|' \
         -e 's|@ar@|$($(package)_ar)|' \
         -e 's|@strip@|$(host_STRIP)|' \
         -e 's|@arch@|$(host_arch)|' \
	  toolchain.txt
endef

define $(package)_config_cmds
  meson setup --cross-file toolchain.txt build
endef

define $(package)_build_cmds
  ninja -C build
endef

define $(package)_stage_cmds
  DESTDIR=$($(package)_staging_dir) ninja -C build install
endef