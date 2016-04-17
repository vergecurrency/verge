
cp src/VERGEd .
strip VERGEd
zip release_${VERGE_PLATFORM}.zip VERGEd

# for pushing releases
brew install ruby
