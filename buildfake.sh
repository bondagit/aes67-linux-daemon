#!/bin/bash
#
# Tested on Ubuntu 18.04
#

#we need clang when compiling on ARMv7
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

TOPDIR=$(pwd)

git config --global http.sslverify false

cd 3rdparty
if [ ! -d ravenna-alsa-lkm ]; then
  git clone --single-branch --branch aes67-daemon https://github.com/bondagit/ravenna-alsa-lkm.git
fi

if [ ! -d cpp-httplib ]; then
  git clone https://github.com/yhirose/cpp-httplib.git
  cd cpp-httplib
  git checkout 42f9f9107f87ad2ee04be117dbbadd621c449552
  cd ..
fi
cd ..

cd daemon
echo "Building aes67-daemon ..."
cmake -DCPP_HTTPLIB_DIR="$TOPDIR"/3rdparty/cpp-httplib -DRAVENNA_ALSA_LKM_DIR="$TOPDIR"/3rdparty/ravenna-alsa-lkm -DENABLE_TESTS=ON -DWITH_AVAHI=OFF -DFAKE_DRIVER=ON .
make
cd ..

