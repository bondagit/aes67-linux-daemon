#!/bin/bash
#
# Tested on Ubuntu 18.04
#

#we need clang when compiling on ARMv7
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

TOPDIR=$(pwd)

cd 3rdparty
if [ ! -d ravenna-alsa-lkm ]; then
  git clone --single-branch --branch aes67-daemon https://github.com/bondagit/ravenna-alsa-lkm.git
  cd ravenna-alsa-lkm/driver
  make
  cd ../..
fi

if [ ! -d cpp-httplib ]; then
  git clone https://github.com/bondagit/cpp-httplib.git
  cd cpp-httplib
  git checkout 42f9f9107f87ad2ee04be117dbbadd621c449552
  cd ..
fi
cd ..

cd webui
echo "Downloading current webui release ..."
wget --timestamping https://github.com/bondagit/aes67-linux-daemon/releases/latest/download/webui.tar.gz
if [ -f webui.tar.gz ]; then
  tar -xzvf webui.tar.gz
else
  echo "Building and installing webui ..."
  # npm install react-modal react-toastify react-router-dom
  npm install
  npm run build
fi
cd ..

cd daemon
echo "Building aes67-daemon ..."
cmake -DCPP_HTTPLIB_DIR="$TOPDIR"/3rdparty/cpp-httplib -DRAVENNA_ALSA_LKM_DIR="$TOPDIR"/3rdparty/ravenna-alsa-lkm -DENABLE_TESTS=ON -DWITH_AVAHI=ON -DFAKE_DRIVER=OFF -DWITH_SYSTEMD=ON .
make
cd ..

