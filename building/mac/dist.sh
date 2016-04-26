
cp src/VERGEd .
cp src/qt/VERGE-qt .
strip VERGEd
strip VERGE-qt
zip release_${VERGE_PLATFORM}.zip VERGEd VERGE-qt
# TODO: Not sure how to do the 'make deploy' from travis... may not be possible :-(

# for pushing releases
brew install ruby
