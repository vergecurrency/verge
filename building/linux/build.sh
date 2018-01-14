./autogen.sh
./configure --with-gui=qt5
make check

# For debugging purposes
cat src/test-suite.log
