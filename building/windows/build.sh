# build the windows exe

./autogen.sh --host=i686-w64-mingw32

env BOOST_ROOT='/tmp/boost_1_60_0' CPPFLAGS='-I/tmp/openssl-1.0.1f/include -I/tmp/db-4.8.30/build_unix' LDFLAGS='-L/tmp/openssl-1.0.1f -L/tmp/db-4.8.30/build_unix/.libs/' ./configure --host=i686-w64-mingw32 
# TODO: need? --enable-cxx

make
