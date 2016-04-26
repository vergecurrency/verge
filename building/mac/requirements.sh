brew install protobuf boost miniupnpc openssl qrencode berkeley-db4 
brew uninstall qt qt5 qt55 qt52

# This next part is to "install" just the necessary parts of the qt5 library
cd /tmp
wget -q https://download.qt.io/official_releases/qt/5.4/5.4.2/qt-opensource-mac-x64-clang-5.4.2.dmg
hdiutil attach -nobrowse /tmp/qt-opensource-mac-x64-clang-5.4.2.dmg
cd /Volumes/qt-opensource-mac-x64-clang-5.4.2/qt-opensource-mac-x64-clang-5.4.2.app/Contents/MacOS/
mkdir -p /usr/local/qt5
./qt-opensource-mac-x64-clang-5.4.2 --runoperation Extract installer://qt.54.clang_64/5.4.2-0qt5_addons.7z /usr/local/qt5
./qt-opensource-mac-x64-clang-5.4.2 --runoperation Extract installer://qt.54.clang_64/5.4.2-0qt5_essentials.7z /usr/local/qt5
./qt-opensource-mac-x64-clang-5.4.2 --runoperation Extract installer://qt.54.qtwebengine.clang_64/5.4.2-0qt5_qtwebengine.7z /usr/local/qt5
./qt-opensource-mac-x64-clang-5.4.2 --runoperation Extract installer://qt.extras.qtcanvas3d.10.src/1.0.0-2qtcanvas3d-opensource-src-1.0.0.7z /usr/local/qt5
./qt-opensource-mac-x64-clang-5.4.2 --runoperation Extract installer://qt.extras.qtwebview.10.src/1.0.0-2qtwebview-opensource-src-1.0.0.7z /usr/local/qt5
./qt-opensource-mac-x64-clang-5.4.2 --runoperation Extract installer://qt.extras.qtwebview.qt54.clang_64/1.0.0-2qt5_qtwebview.7z /usr/local/qt5
./qt-opensource-mac-x64-clang-5.4.2 --runoperation Extract installer://qt.license.thirdparty/1.0.0ThirdPartySoftware_Listing.7z /usr/local/qt5
./qt-opensource-mac-x64-clang-5.4.2 --runoperation Extract installer://qt.license.thirdparty/1.0.0fdl_license.7z /usr/local/qt5
./qt-opensource-mac-x64-clang-5.4.2 --runoperation QtPatch mac /usr/local/qt5/5.4/clang_64 qt5
# unmount volume
hdiutil detach /Volumes/qt-opensource-mac-x64-clang-5.4.2

