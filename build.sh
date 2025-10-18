#!/bin/bash
#
# Tested on Ubuntu 18.04
#

#we need clang when compiling on ARMv7
#export CC=/usr/bin/clang
#export CXX=/usr/bin/clang++

TOPDIR=$(pwd)

echo "Init git submodules ..."
git submodule update --init --recursive

cd 3rdparty/ravenna-alsa-lkm/driver
git checkout aes67-daemon
make
cd -

cd 3rdparty/whisper.cpp/models
if [ ! -f ggml-base.en.bin ]; then
./download-ggml-model.sh base.en
fi
cd -
cd 3rdparty/whisper.cpp
cmake -B build
cmake --build build --config Release
cd -

cd webui
#echo "Downloading current webui release ..."
#wget --timestamping https://github.com/bondagit/aes67-linux-daemon/releases/latest/download/webui.tar.gz
#if [ -f webui.tar.gz ]; then
#  tar -xzvf webui.tar.gz
#else
  echo "Building and installing webui ..."
  # npm install react-modal react-toastify react-router-dom
  npm install
  npm ci
  npm run build
#fi
cd ..

cd daemon

echo "Building aes67-daemon ..."
cmake \
	-DBoost_NO_WARN_NEW_VERSIONS=1 \
	-DCPP_HTTPLIB_DIR="${TOPDIR}/3rdparty/cpp-httplib" \
	-DRAVENNA_ALSA_LKM_DIR="${TOPDIR}/3rdparty/ravenna-alsa-lkm" \
	-DWHISPER_CPP_DIR="${TOPDIR}/3rdparty/whisper.cpp" \
	-DENABLE_TESTS=ON \
	-DWITH_AVAHI=ON \
	-DFAKE_DRIVER=OFF \
	-DWITH_SYSTEMD=ON \
	-DWITH_STREAMER=ON \
	-DWITH_TRANSCRIBER=ON \
	.
make
cd ..
cd test
make
cd ..

