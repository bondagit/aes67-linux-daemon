#!/bin/bash
#
# Tested on Ubuntu 22.04 LTS
#

sudo apt update
sudo apt -y install psmisc build-essential clang git cmake libboost-all-dev valgrind linux-sound-base alsa-base alsa-utils libasound2-dev linuxptp libavahi-client-dev linux-headers-$(uname -r)
