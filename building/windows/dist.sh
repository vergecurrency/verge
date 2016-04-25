
cp src/VERGEd.exe .
cp src/VERGEd-qt.exe .
strip VERGEd.exe
strip VERGEd-qt.exe
zip release_windows.zip VERGEd.exe VERGEd-qt.exe

# for pushing releases
sudo apt-get --yes -qq install ruby curl > /dev/null
gpg --keyserver hkp://keys.gnupg.net --recv-keys 409B6B1796C275462A1703113804BB82D39DC0E3
\curl -sSL https://get.rvm.io | bash -s stable
source /etc/profile.d/rvm.sh
rvm reload
rvm install 2.2.3
rvm install 1.9.3
rvm use 2.2.3
export PATH=/usr/local/rvm/gems/ruby-2.2.3/bin:$PATH
