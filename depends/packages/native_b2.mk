package=native_b2
boost_package=boost
$(package)_version=$(boost_version)
$(package)_download_path=$(boost_download_path)
$(package)_file_name=$(boost_file_name)
$(package)_sha256_hash=$(boost_sha256_hash)
$(package)_build_subdir=tools/build/src/engine
$(package)_darwin_dependencies=native_clang

define $(package)_fetch_cmds
$(call fetch_file,$(package),$($(package)_download_path),$($(package)_download_file),$($(package)_file_name),$($(package)_sha256_hash))
endef

define $(package)_build_cmds
  CXX="$($(boost_package)_cxx)" CXXFLAGS="$($(package)_cxxflags)" ./build.sh "$($(boost_package)_toolset_$(host_os))"
endef

define $(package)_stage_cmds
  mkdir -p "$($(package)_staging_prefix_dir)"/bin/ && \
  cp b2 "$($(package)_staging_prefix_dir)"/bin/
endef