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
sudo apt-get install -y npm
sudo apt-get install -y libboost-all-dev
sudo apt-get install -y valgrind
sudo apt-get install -y linux-sound-base alsa-base alsa-utils
sudo apt-get install -y linuxptp
sudo apt install -y linux-headers-$(uname -r)

