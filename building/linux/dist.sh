
cp src/VERGEd .
zip release.zip VERGEd

# for pushing releases
sudo apt-get --yes install ruby curl
gpg --keyserver hkp://keys.gnupg.net --recv-keys 409B6B1796C275462A1703113804BB82D39DC0E3
\curl -sSL https://get.rvm.io | bash -s stable
source /etc/profile.d/rvm.sh
rvm install 2.2.3
rvm use 2.2.3

