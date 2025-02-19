package=native_libtapi
$(package)_version=54c9044082ba35bdb2b0edf282ba1a340096154c
$(package)_download_path=https://github.com/tpoechtrager/apple-libtapi/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=d4d46c64622f13d6938cecf989046d9561011bb59e8ee835f8f39825d67f578f
$(package)_patches=disable_zlib.patch

ifeq ($(strip $(FORCE_USE_SYSTEM_CLANG)),)
$(package)_dependencies=native_llvm
endif

define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/disable_zlib.patch
endef

define $(package)_build_cmds
  CC=$(clang_prog) CXX=$(clangxx_prog) INSTALLPREFIX=$($(package)_staging_prefix_dir) ./build.sh
endef

define $(package)_stage_cmds
  ./install.sh
endef
