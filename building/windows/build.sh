# build the windows exe

./autogen.sh --host=i686-w64-mingw32

#env BOOST_ROOT='/tmp/boost_1_55_0' 
env CPPFLAGS='-I/tmp/openssl-1.0.1f/include -I/tmp/db-4.8.30/build_unix -I/tmp/boost_1_5t_0' LDFLAGS='-L/tmp/openssl-1.0.1f -L/tmp/db-4.8.30/build_unix/.libs -L/tmp/boost_1_55_0/stage/lib' ./configure --host=i686-w64-mingw32 --with-boost-libdir=/tmp/boost_1_55_0/stage/lib
#env CPPFLAGS='-I/tmp/openssl-1.0.1f/include -I/tmp/db-4.8.30/build_unix -I/tmp/boost_1_60_0' LDFLAGS='-L/tmp/openssl-1.0.1f -L/tmp/db-4.8.30/build_unix/.libs -L/tmp/boost_1_60_0/stage/lib -L/tmp/boost_1_60_0/bin.v2/libs/log/build/gcc-mingw-4.9.2/release/binary-format-pe/build-no/link-static/target-os-windows/threading-multi/ -L/tmp/boost_1_60_0/bin.v2/libs/chrono/build/gcc-mingw-4.9.2/release/binary-format-pe/link-static/target-os-windows/threading-multi/' ./configure --host=i686-w64-mingw32 --with-boost-libdir=/tmp/boost_1_60_0/stage/lib
#--enable-dht 
# TODO: need? --enable-cxx

make
