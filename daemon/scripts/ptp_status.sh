#!/bin/bash

if [ $1 == "locked" ]; then
  echo "$0 >> PTP locked";
  #speaker-test -D plughw:RAVENNA -r 48000 -c 2 -t sine &
elif [ $1 == "locking" ]; then
  echo "$0 >> PTP locking";
elif [ $1 == "unlocked" ]; then
  echo "$0 >> PTP unlocked";
  #killall -9 speaker-test
fi
