packages:=boost openssl libevent libcap libseccomp zeromq

qt_native_packages = native_protobuf
qt_packages = qrencode protobuf zlib

qt_linux_packages:=qt expat libxcb libxcb_util xcb_proto libXau xorgproto freetype fontconfig libX11 xextproto libXext xtrans

qt_darwin_packages=qt
qt_mingw32_packages=qt

wallet_packages=bdb

upnp_packages=miniupnpc

darwin_native_packages = native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_cdrkit native_libdmg-hfsplus native_libtapi
endif
