#!/bin/bash
#
# Tested on Ubuntu 18.04
#

sudo apt update
sudo apt-get install -y psmisc
sudo apt-get install -y build-essential
sudo apt-get install -y clang
sudo apt-get install -y git
sudo apt-get install -y cmake
# sudo apt-get install -y npm
sudo apt-get install -y libboost-all-dev
sudo apt-get install -y valgrind
sudo apt-get install -y linux-sound-base
sudo apt-get install -y alsa-base
sudo apt-get install -y alsa-utils
sudo apt-get install -y libasound2-dev
sudo apt-get install -y linuxptp
sudo apt-get install -y libavahi-client-dev
sudo apt install -y linux-headers-$(uname -r)
sudo apt-get install -y libsystemd-dev
sudo apt-get install -y libfaac-dev
