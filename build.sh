#!/bin/bash
#
# Tested on Ubuntu 18.04
#

#we need clang when compiling on ARMv7
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

cd 3rdparty
if [ ! -d ravenna-alsa-lkm.git ]; then
  git clone https://bitbucket.org/MergingTechnologies/ravenna-alsa-lkm.git
  cd ravenna-alsa-lkm
  git checkout 35c708f3747474130790cf508c064360a9589ac8
  cd driver
  echo "Apply patches to ravenna-alsa-lkm module ..."
  git apply ../../patches/ravenna-alsa-lkm-kernel-v5.patch
  git apply ../../patches/ravenna-alsa-lkm-enable-loopback.patch  
  git apply ../../patches/ravenna-alsa-lkm-fixes.patch
  git apply ../../patches/ravenna-alsa-lkm-arm-32bit.patch
  git apply ../../patches/ravenna-alsa-lkm-add-codec-am824.patch
  git apply ../../patches/ravenna-alsa-lkm-disable-ptp-checksum.patch
  echo "Building ravenna-alsa-lkm kernel module ..."
  make
  cd ../..
fi

if [ ! -d cpp-httplib.git ]; then
  git clone https://github.com/yhirose/cpp-httplib.git
  cd cpp-httplib
  git checkout 42f9f9107f87ad2ee04be117dbbadd621c449552
  cd ..
fi
cd ..

cd webui
echo "Building and installing webui ..."
#npm install react-modal react-toastify react-router-dom
npm install
npm run build
cd ..

cd daemon
echo "Building aes67-daemon ..."
cmake -DWITH_AVAHI=ON .
make
cd ..

