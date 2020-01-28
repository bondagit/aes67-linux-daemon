#!/bin/bash
#
# Tested on Ubuntu 18.04
#

cd 3rdparty
if [ ! -d ravenna-alsa-lkm.git ]; then
  git clone https://bitbucket.org/MergingTechnologies/ravenna-alsa-lkm.git
  cd ravenna-alsa-lkm
  git checkout 5a06f0d33c18e532eb5dac3ad90c0acd59fbabd7
  cd driver
  echo "Apply patches to ravenna-alsa-lkm module ..."
  git apply ../../patches/ravenna-alsa-lkm-kernel-v5.patch
  git apply ../../patches/ravenna-alsa-lkm-enable-loopback.patch  
  git apply ../../patches/ravenna-alsa-lkm-fixes.patch
  echo "Building ravenna-alsa-lkm kernel module ..."
  make
  cd ../..
fi

if [ ! -d cpp-httplib.git ]; then
  git clone https://github.com/yhirose/cpp-httplib.git
  cd cpp-httplib
  git checkout 301a419c0243d3ab843e5fc2bb9fa56a9daa7bcd
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
cmake .
make
cd ..

