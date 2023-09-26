package=openssl
$(package)_version=3.0.0
$(package)_download_path=https://www.openssl.org/source
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=59eedfcb46c25214c9bd37ed6078297b4df01d012267fe9e9eee31f61bc70536

define $(package)_set_vars
$(package)_config_env=AR="$($(package)_ar)" RANLIB="$($(package)_ranlib)" CC="$($(package)_cc)" CFLAGS="$($(package)_cflags)"
$(package)_config_opts=--prefix=$(host_prefix) --openssldir=$(host_prefix)/etc/openssl --api=3.0 
$(package)_config_opts+=no-shared
$(package)_config_opts+=no-acvp-tests
$(package)_config_opts+=no-afalgeng
$(package)_config_opts+=no-async
$(package)_config_opts+=no-bf
$(package)_config_opts+=no-blake2
$(package)_config_opts+=no-capieng
$(package)_config_opts+=no-cast
$(package)_config_opts+=no-cmac
$(package)_config_opts+=no-cms
$(package)_config_opts+=no-comp
$(package)_config_opts+=no-ct
$(package)_config_opts+=no-deprecated
$(package)_config_opts+=no-des
$(package)_config_opts+=no-dgram
$(package)_config_opts+=no-dsa
$(package)_config_opts+=no-dso
$(package)_config_opts+=no-dtls
$(package)_config_opts+=no-dynamic-engine
$(package)_config_opts+=no-ec2m
$(package)_config_opts+=no-engine
$(package)_config_opts+=no-err
$(package)_config_opts+=no-gost
$(package)_config_opts+=no-idea
$(package)_config_opts+=no-legacy
$(package)_config_opts+=no-md4
$(package)_config_opts+=no-mdc2
$(package)_config_opts+=no-multiblock
$(package)_config_opts+=no-nextprotoneg
$(package)_config_opts+=no-ocb
$(package)_config_opts+=no-ocsp
$(package)_config_opts+=no-poly1305
$(package)_config_opts+=no-posix-io
$(package)_config_opts+=no-psk
$(package)_config_opts+=no-rc2
$(package)_config_opts+=no-rc4
$(package)_config_opts+=no-rdrand
$(package)_config_opts+=no-rfc3779
$(package)_config_opts+=no-rmd160
$(package)_config_opts+=no-scrypt
$(package)_config_opts+=no-seed
$(package)_config_opts+=no-sock
$(package)_config_opts+=no-srp
$(package)_config_opts+=no-srtp
$(package)_config_opts+=no-ssl
$(package)_config_opts+=no-ssl-trace
$(package)_config_opts+=no-stdio
$(package)_config_opts+=no-tests
$(package)_config_opts+=no-tls
$(package)_config_opts+=no-ts
$(package)_config_opts+=no-whirlpool
$(package)_config_opts+=-DPURIFY
$(package)_config_opts_linux=-fPIC -Wa,--noexecstack
$(package)_config_opts_x86_64_linux=linux-x86_64
$(package)_config_opts_i686_linux=linux-generic32
$(package)_config_opts_arm_linux=linux-generic32
$(package)_config_opts_aarch64_linux=linux-generic64
$(package)_config_opts_mipsel_linux=linux-generic32
$(package)_config_opts_mips_linux=linux-generic32
$(package)_config_opts_powerpc_linux=linux-generic32
$(package)_config_opts_x86_64_darwin=darwin64-x86_64-cc
$(package)_config_opts_x86_64_mingw32=mingw64
$(package)_config_opts_i686_mingw32=mingw
endef

define $(package)_preprocess_cmds
#  sed -i.old "/define DATE/d" util/mkbuildinf.pl && \
  sed -i.old "s|\"engines\", \"apps\", \"test\"|\"engines\"|" Configure
endef

define $(package)_config_cmds
  ./Configure $($(package)_config_opts)
endef

define $(package)_build_cmds
  $(MAKE) -j1 build_libs build_libs_nodep
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) -j$(JOBCOUNT) install_sw
endef

define $(package)_postprocess_cmds
  rm -rf share bin etc
endef
