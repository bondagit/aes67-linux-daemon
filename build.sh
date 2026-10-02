#!/bin/bash
#
# Tested on Ubuntu 18.04
#

#we need clang when compiling on ARMv7
#export CC=/usr/bin/clang
#export CXX=/usr/bin/clang++

TOPDIR=$(pwd -P)
REPO_TOP=$(git rev-parse --show-toplevel 2>/dev/null)
if [ $? -ne 0 ] || [ "$(cd "$REPO_TOP" 2>/dev/null && pwd -P)" != "$TOPDIR" ]; then
  echo "ERROR: build.sh must be run from the root of a Git checkout."
  echo "GitHub source archives do not contain the required submodules."
  echo "Clone the repository with:"
  echo "  git clone --recurse-submodules https://github.com/bondagit/aes67-linux-daemon.git"
  exit 1
fi

echo "Init git submodules ..."
if ! git submodule update --init --recursive; then
  echo "ERROR: failed to initialize required Git submodules."
  exit 1
fi

if [ ! -f 3rdparty/ravenna-alsa-lkm/driver/RTP_stream_info.h ]; then
  echo "ERROR: 3rdparty/ravenna-alsa-lkm is incomplete: driver/RTP_stream_info.h is missing."
  echo "Run: git submodule update --init --recursive"
  exit 1
fi

cd 3rdparty/ravenna-alsa-lkm/driver || exit 1
git checkout aes67-daemon || exit 1

# Use clang for kernel 7.2+
KERNEL_VERSION=$(uname -r | cut -d. -f1,2 | tr -d '.')
if [ "$KERNEL_VERSION" -ge 72 ]; then
  make CC=clang
else
  make
fi
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
	-DBoost_NO_WARN_NEW_VERSIONS=1 \
	-DCPP_HTTPLIB_DIR="${TOPDIR}/3rdparty/cpp-httplib" \
	-DRAVENNA_ALSA_LKM_DIR="${TOPDIR}/3rdparty/ravenna-alsa-lkm" \
	-DENABLE_TESTS=ON \
	-DWITH_AVAHI=ON \
	-DFAKE_DRIVER=OFF \
	-DWITH_SYSTEMD=ON \
	-DWITH_STREAMER=ON \
	-DWITH_NMOS=ON \
	.
make
cd ..
cd test
make
cd ..

