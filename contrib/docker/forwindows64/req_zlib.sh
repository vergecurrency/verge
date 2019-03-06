
echo "=== Building ZLIB now..."
cd /tmp
wget http://zlib.net/fossils/zlib-1.2.11.tar.gz
tar xvfz zlib-1.2.11.tar.gz
cd zlib-1.2.11
#sed -e s/"PREFIX ="/"PREFIX = i686-w64-mingw32.static-"/ -i win32/Makefile.gcc

# Use it if you want to build for x64
sed -e s/"PREFIX ="/"PREFIX = x86_64-w64-mingw32-"/ -i win32/Makefile.gcc

# Use it if you want to build for x32
# sed -e s/"PREFIX ="/"PREFIX = i686-w64-mingw32-"/ -i win32/Makefile.gcc

make -f win32/Makefile.gcc 
echo "=== done building ZLIB ..."
