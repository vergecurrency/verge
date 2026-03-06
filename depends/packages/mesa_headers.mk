package=mesa_headers
$(package)_version=24.3.3
$(package)_download_path=https://mesa.freedesktop.org/archive
$(package)_file_name=mesa-$($(package)_version).tar.xz
$(package)_sha256_hash=105afc00a4496fa4d29da74e227085544919ec7c86bd92b0b6e7fcc32c7125f4

define $(package)_config_cmds
  true
endef

define $(package)_build_cmds
  true
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include && \
  cp -a include/KHR $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp -a include/EGL $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp -a include/GLES $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp -a include/GLES2 $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp -a include/GLES3 $($(package)_staging_dir)$(host_prefix)/include/ && \
  cp -a include/GL $($(package)_staging_dir)$(host_prefix)/include/
endef
