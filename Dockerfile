# Dockerfile for Verge
# http://vergecurrency.com/
# https://bitcointalk.org/index.php?topic=1365894
# https://github.com/vergecurrency/verge

# https://github.com/vergecurrency/Docker-Verge-Daemon.git
#  Jeremiah Buddenhagen <bitspill@bitspill.net>

FROM ubuntu:18.04

MAINTAINER Marvin Piekarek (marpme) <marvin.piekarek@gmail.com> 

RUN apt-get update && apt-get install git ruby apt-cacher-ng qemu-utils debootstrap lxc python-cheetah parted kpartx bridge-utils make ubuntu-archive-keyring curl firewalld

WORKDIR /gitian
COPY . /verge

COPY ./contrib/gitian-build.py .
RUN chmod u+x gitian-build.py
RUN ./gitian-build.py --setup marpme master
