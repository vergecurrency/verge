package=native_cctools
$(package)_version=55943b0c68c0eaf8b8ad2f51f63738bbc7b0c86b
$(package)_download_path=https://github.com/tpoechtrager/cctools-port/archive
$(package)_file_name=$($(package)_version).tar.gz
#$(package)_sha256_hash=e2c1588d505a69c32e079f4e616e0f117d5478429040e394f624f43f2796e6bc
$(package)_build_subdir=cctools
$(package)_dependencies=native_libtapi

define $(package)_set_vars
  $(package)_config_opts=--target=$(host) --enable-lto-support
  $(package)_config_opts+=--with-llvm-config=$(llvm_config_prog)
  $(package)_ldflags+=-Wl,-rpath=\\$$$$$$$$\$$$$$$$$ORIGIN/../lib
  $(package)_cc=$(clang_prog)
  $(package)_cxx=$(clangxx_prog)
endef

ifneq ($(strip $(FORCE_USE_SYSTEM_CLANG)),)
define $(package)_preprocess_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib && \
  cp $(llvm_lib_dir)/libLTO.so $($(package)_staging_prefix_dir)/lib/
endef
else
endif

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
