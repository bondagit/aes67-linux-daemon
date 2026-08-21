#!/bin/bash
sudo apt update
sudo apt install -y \
  git cmake g++ ninja-build \
  libboost-all-dev \
  libssl-dev \
  libcurl4-openssl-dev \
  libcpprest-dev \
  libwebsocketpp-dev \
  libavahi-compat-libdnssd-dev \
  pkg-config

mkdir -p build/
cd build
git clone https://github.com/pboettch/json-schema-validator.git
cd json-schema-validator/
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
sudo ldconfig
cd ..
git clone https://github.com/Thalhammer/jwt-cpp.git
cd jwt-cpp/
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
sudo cmake --install build
sudo ldconfig
cd ..
cmake ../Development/ -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)

