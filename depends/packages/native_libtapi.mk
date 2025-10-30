package=native_libtapi
$(package)_version=aed9334283e3e290bba622ee980bde2322e4d516
$(package)_download_path=https://github.com/tpoechtrager/apple-libtapi/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=F244F7B3D8DE3277204DECE255B531F976BEE80DAD3247E9F36B30052B91C959
$(package)_patches=disable_zlib.patch no_embed_git_rev.patch

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
