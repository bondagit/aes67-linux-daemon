#!/bin/bash

rm -rf 3rdparty/ravenna-alsa-lkm
rm -rf 3rdparty/cpp-httplib

rm -rf daemon/status.json
rm -rf daemon/CMakeFiles
rm -rf daemon/Makefile
rm -rf daemon/CMakeCache.txt
rm -rf daemon/cmake_install.cmake
rm -rf daemon/CTestTestfile.cmake
rm -rf daemon/Testing
rm -rf daemon/aes67-daemon
rm -rf daemon/tests/CMakeFiles
rm -rf daemon/tests/Makefile
rm -rf daemon/tests/CMakeCache.txt
rm -rf daemon/tests/cmake_install.cmake
rm -rf daemon/tests/CTestTestfile.cmake
rm -rf daemon/tests/Testing
rm -rf daemon/tests/daemon-test

rm -rf demo/sink-test.wav

rm -rf webui/build
rm -rf webui/node_modules
rm -rf webui/*.log
rm -rf webui/package-lock.json

