
echo "=== Building ZLIB now..."
cd /tmp
wget http://zlib.net/zlib-1.2.8.tar.gz
tar xvfz zlib-1.2.8.tar.gz
cd zlib-1.2.8
sed -e s/"PREFIX ="/"PREFIX = i686-w64-mingw32.static-"/ -i win32/Makefile.gcc
make -f win32/Makefile.gcc 
echo "=== done building ZLIB ..."
