package=miniupnpc
$(package)_version=2.3.0
$(package)_download_path=https://miniupnp.tuxfamily.org/files/
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=025C9AB95677F02A69BC64AC0A747F07E02BA99CF797BC679A5A552FED8D990C
$(package)_patches=dont_leak_info.patch fix_windows_build.patch

define $(package)_set_vars
$(package)_build_opts=CC="$($(package)_cc)"
$(package)_build_opts_mingw32=-f Makefile.mingw CFLAGS="$($(package)_cflags)"
$(package)_build_env+=CFLAGS="$($(package)_cflags) $($(package)_cppflags)" AR="$($(package)_ar)"
endef

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/dont_leak_info.patch && \
  patch -p1 < $($(package)_patch_dir)/fix_windows_build.patch
endef

define $(package)_build_cmds
	$(MAKE) build/libminiupnpc.a $($(package)_build_opts)
endef

define $(package)_stage_cmds
	mkdir -p $($(package)_staging_prefix_dir)/include/miniupnpc $($(package)_staging_prefix_dir)/lib &&\
	install include/*.h $($(package)_staging_prefix_dir)/include/miniupnpc &&\
	install build/libminiupnpc.a $($(package)_staging_prefix_dir)/lib
endef
