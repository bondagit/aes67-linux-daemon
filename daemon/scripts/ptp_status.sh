#!/bin/bash
#
if [ -z "$1" ]; then
  echo "Usage: $0 {locked|locking|unlocked}"
  exit 1
fi

if [ $1 == "locked" ]; then
  echo "$0 >> PTP locked";
#  /usr/bin/arecord -D plughw:RAVENNA -c 2 -f cd -d 30 -r 48000 -t wav sink.wav &
#  /usr/bin/speaker-test -D plughw:RAVENNA -r 48000 -c 2 -t sine &
elif [ $1 == "locking" ]; then
  echo "$0 >> PTP locking";
elif [ $1 == "unlocked" ]; then
  echo "$0 >> PTP unlocked";
#  killall -9 arecord speaker-test
fi
