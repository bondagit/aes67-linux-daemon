#!/bin/bash
#
# Tested on Ubuntu 18.04
#

#we need clang when compiling on ARMv7
#export CC=/usr/bin/clang
#export CXX=/usr/bin/clang++

die() {
	echo >&2 "ERROR: $*"
	exit 1
}

TOPDIR=$(pwd)

echo "Init git submodules ..."
git submodule update --init --recursive

cd 3rdparty/ravenna-alsa-lkm/driver
git checkout aes67-daemon
make || die "Building the ALSA kernel module failed"
cd -

cd webui
echo "Downloading current webui release ..."
wget --timestamping https://github.com/bondagit/aes67-linux-daemon/releases/latest/download/webui.tar.gz
if [ -f webui.tar.gz ]; then
  tar -xzvf webui.tar.gz
else
  echo "Building and installing webui ..."
  # npm install react-modal react-toastify react-router-dom
  npm ci
  npm run build
fi
cd ..

cd daemon
echo "Building aes67-daemon ..."
cmake \
	-DCPP_HTTPLIB_DIR="${TOPDIR}/3rdparty/cpp-httplib" \
	-DRAVENNA_ALSA_LKM_DIR="${TOPDIR}/3rdparty/ravenna-alsa-lkm" \
	-DENABLE_TESTS=ON \
	-DWITH_AVAHI=ON \
	-DFAKE_DRIVER=OFF \
	-DWITH_SYSTEMD=ON \
	-DWITH_STREAMER=ON \
	.
make
cd ..
cd test
make
cd ..

