
cp src/VERGEd .
strip VERGEd
zip release_${VERGE_PLATFORM}.zip VERGEd VERGE-Qt.app 

# for pushing releases
brew install ruby
