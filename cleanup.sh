#!/bin/bash

rm -rf 3rdparty/ravenna-alsa-lkm
rm -rf 3rdparty/cpp-httplib

rm -f daemon/status.json
rm -f daemon/CMakeFiles
rm -f daemon/Makefile
rm -f daemon/CMakeCache.txt
rm -f daemon/cmake_install.cmake
rm -f daemon/CTestTestfile.cmake
rm -f daemon/Testing
rm -f daemon/aes67-daemon
rm -f daemon/tests/CMakeFiles
rm -f daemon/tests/Makefile
rm -f daemon/tests/CMakeCache.txt
rm -f daemon/tests/cmake_install.cmake
rm -f daemon/tests/CTestTestfile.cmake
rm -f daemon/tests/Testing
rm -f daemon/tests/daemon-test

rm -f demo/sink-test.wav

rm -f webui/build
rm -f webui/node_modules
rm -f webui/*.log
rm -f webui/package-lock.json

