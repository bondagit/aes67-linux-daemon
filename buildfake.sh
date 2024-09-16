#!/bin/bash
#
# Tested on Ubuntu 18.04
#

#we need clang when compiling on ARMv7
export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

TOPDIR=$(pwd)

git config --global http.sslverify false
git submodule update --init --recursive

cd daemon
echo "Building aes67-daemon ..."
cmake \
	-DCPP_HTTPLIB_DIR="${TOPDIR}/3rdparty/cpp-httplib" \
	-DRAVENNA_ALSA_LKM_DIR="${TOPDIR}/3rdparty/ravenna-alsa-lkm" \
	-DENABLE_TESTS=ON \
	-DWITH_AVAHI=OFF \
	-DFAKE_DRIVER=ON \
	-DWITH_STREAMER=OFF \
	.
make
cd ..

