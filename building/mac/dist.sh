
cp src/VERGEd .
cp src/qt/VERGEd-qt .
strip VERGEd
strip VERGEd-qt
zip release_${VERGE_PLATFORM}.zip VERGEd VERGEd-qt

# for pushing releases
brew install ruby
