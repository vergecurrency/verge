package=miniupnpc
$(package)_version=2.2.7
$(package)_download_path=http://miniupnp.free.fr/files/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=b0c3a27056840fd0ec9328a5a9bac3dc5e0ec6d2e8733349cf577b0aa1e70ac1
$(package)_patches=dont_leak_info.patch cmake_get_src_addr.patch fix_windows_snprintf.patch

# Next time this package is updated, ensure that _WIN32_WINNT is still properly set.
# See discussion in https://github.com/bitcoin/bitcoin/pull/25964.
define $(package)_set_vars
$(package)_build_opts=CC="$($(package)_cc)"
$(package)_build_opts_mingw32=-f Makefile.mingw CFLAGS="$($(package)_cflags) -D_WIN32_WINNT=0x0601"
$(package)_build_env+=CFLAGS="$($(package)_cflags) $($(package)_cppflags)" AR="$($(package)_ar)"
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/dont_leak_info.patch && \
  patch -p1 < $($(package)_patch_dir)/cmake_get_src_addr.patch && \
  patch -p1 < $($(package)_patch_dir)/fix_windows_snprintf.patch
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
  cmake --install . --prefix $($(package)_staging_prefix_dir)
endef
