package=nss
$(package)_version=3.104
$(package)_download_path=https://archive.mozilla.org/pub/security/nss/releases/NSS_$(subst .,_,$($(package)_version))_RTM/src
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=e2763223622d1e76b98a43030873856f248af0a41b03b2fa2ca06a91bc50ac8e
$(package)_dependencies=nspr

define $(package)_config_cmds
  true
endef

define $(package)_build_cmds
  cd nss && \
  ./build.sh -o --disable-tests --python=python3 --with-nspr=$(host_prefix)/include/nspr:$(host_prefix)/lib -Ddisable_werror=1 -Dmozilla_client=1
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_dir)$(host_prefix)/include/nss && \
  mkdir -p $($(package)_staging_dir)$(host_prefix)/lib/pkgconfig && \
  cp -a dist/public/nss/* $($(package)_staging_dir)$(host_prefix)/include/nss/ && \
  cp -a dist/Release/lib/libnss3.so* $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp -a dist/Release/lib/libnssutil3.so* $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp -a dist/Release/lib/libsmime3.so* $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp -a dist/Release/lib/libssl3.so* $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp -a dist/Release/lib/libfreebl3.so* $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp -a dist/Release/lib/libsoftokn3.so* $($(package)_staging_dir)$(host_prefix)/lib/ && \
  cp -a dist/Release/lib/libnssckbi.so* $($(package)_staging_dir)$(host_prefix)/lib/ && \
  printf 'prefix=%s\nexec_prefix=$${prefix}\nlibdir=$${exec_prefix}/lib\nincludedir=$${prefix}/include\n\nName: NSS\nDescription: Network Security Services\nVersion: %s\nRequires: nspr\nLibs: -L$${libdir} -lnss3 -lnssutil3 -lsmime3 -lssl3\nCflags: -I$${includedir}/nss\n' \
    '$(host_prefix)' '$($(package)_version)' > $($(package)_staging_dir)$(host_prefix)/lib/pkgconfig/nss.pc
endef
