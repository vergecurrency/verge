
export PKG_CONFIG_PATH=/usr/local/opt/qt@5.10.0/lib/pkgconfig
export PATH=/usr/local/opt/qt@5.10.0/bin:$PATH
export QT_CFLAGS="-I/usr/local/opt/qt@5.10.0/lib/QtWebKitWidgets.framework/Versions/5/Headers -I/usr/local/opt/qt@5.10.0/lib/QtDBus.framework/Versions/5/Headers -I/usr/local/opt/qt@5.10.0/lib/QtWidgets.framework/Versions/5/Headers -I/usr/local/opt/qt@5.10.0/lib/QtWebKit.framework/Versions/5/Headers -I/usr/local/opt/qt@5.10.0/lib/QtNetwork.framework/Versions/5/Headers -I/usr/local/opt/qt@5.10.0/lib/QtGui.framework/Versions/5/Headers -I/usr/local/opt/qt@5.10.0/lib/QtCore.framework/Versions/5/Headers -I. -I/usr/local/opt/qt@5.10.0/mkspecs/macx-clang -F/usr/local/opt/qt@5.10.0/lib"
export QT_LIBS="-F/usr/local/opt/qt@5.10.0/lib -framework QtWidgets -framework QtGui -framework QtCore -framework DiskArbitration -framework IOKit -framework OpenGL -framework AGL -framework QtNetwork -framework QtWebKit -framework QtWebKitWidgets -framework QtDBus"

./autogen.sh
./configure --with-gui=qt5
make
