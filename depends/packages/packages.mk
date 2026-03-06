packages:=openssl libevent libcap libseccomp zeromq boost

qt_native_packages = native_protobuf native_flex native_webencodings native_html5lib
ifneq ($(host),$(build))
qt_native_packages += native_qt
endif
qt_packages = qrencode protobuf zlib

qt_linux_packages:=qt expat dbus nspr nss mesa_headers libglvnd libX11 libXext libxcb libxkbcommon libxcb_util xcb_proto libXau freetype fontconfig xextproto xtrans libxcb_util_render libxcb_util_keysyms xproto libxcb_util_image libxcb_util_wm libxcb_util_cursor util-macros

qt_darwin_packages=qt
qt_mingw32_packages=qt

wallet_packages=bdb

upnp_packages=miniupnpc

darwin_native_packages = native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_cdrkit native_libdmg-hfsplus native_libtapi
endif
