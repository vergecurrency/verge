
cp src/VERGEd .
cp src/qt/VERGE-qt .
strip VERGEd
strip VERGE-qt
zip release_${VERGE_PLATFORM}.zip VERGEd VERGE-qt

sudo easy_install appscript

make deploy
ls -al VER*

# for pushing releases
brew install ruby
