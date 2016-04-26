
cp src/VERGEd .
cp src/qt/VERGE-qt .
strip VERGEd
strip VERGE-qt
zip release_${VERGE_PLATFORM}.zip VERGEd VERGE-qt

# doubt this will work... but will give it a shot
make deploy

# for pushing releases
brew install ruby
