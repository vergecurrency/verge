
cp src/VERGEd .
strip VERGEd
zip release_${VERGE_PLATFORM}.zip VERGEd 
# TODO: Not sure how to do the 'make deploy' from travis... may not be possible :-(

# for pushing releases
brew install ruby
