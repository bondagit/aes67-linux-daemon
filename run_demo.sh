#!/bin/bash
#
# Tested on Ubuntu 18.04
#

function cleanup {
#kill and wait for previous daemon instances to exit
  sudo killall -q ptp4l
  killall -q aes67-daemon
  while killall -0 aes67-daemon 2>/dev/null ; do
    sleep 1
  done
}

if ! [ -x "$(command -v ptp4l)" ]; then
  echo 'Error: ptp4l is not installed.' >&2
  exit 1
fi

if ! [ -x "$(command -v arecord)" ]; then
  echo 'Error: arecord is not installed.' >&2
  exit 1
fi

if ! [ -x "$(command -v speaker-test)" ]; then
  echo 'Error: speaker-test is not installed.' >&2
  exit 1
fi

if ! [ -x "$(command -v ./daemon/aes67-daemon)" ]; then
  echo 'Error: aes67-daemon is not compiled.' >&2
  exit 1
fi

if ! [ -r "3rdparty/ravenna-alsa-lkm/driver/MergingRavennaALSA.ko" ]; then
  echo 'Error: MergingRavennaALSA.ko module is not compiled.' >&2
  exit 1
fi

trap cleanup EXIT

#configure system parms
sudo sysctl -w net/ipv4/igmp_max_memberships=64

if [ -x /usr/bin/pulseaudio ]; then
  #stop pulseaudio, this seems to open/close ALSA continuosly
  echo autospawn = no > $HOME/.config/pulse/client.conf
  pulseaudio --kill >/dev/null 2>&1
  rm $HOME/.config/pulse/client.conf
  #disable pulseaudio
  systemctl --user stop pulseaudio.socket > /dev/null 2>&1
  systemctl --user stop pulseaudio.sservice > /dev/null 2>&1
fi

#install kernel module
sudo insmod 3rdparty/ravenna-alsa-lkm/driver/MergingRavennaALSA.ko

cleanup
if [ -f ./demo/sink_test.wav ] ; then
  rm -f sink_test.wav
fi

echo "Starting PTP master ..."
sudo ptp4l -i lo -l7 -E -S &

#echo "Starting AES67 daemon ..."
./daemon/aes67-daemon -c ./demo/daemon.conf &

#open browser on configuration page
if [ -x "$(command -v xdg-open)" ]; then
  xdg-open "http://127.0.0.1:8080/PTP"
fi

echo "Waiting for PTP slave to sync ..."
sleep 30

#starting recording on sink
echo "Starting to record 60 secs from sink ..."
arecord -D plughw:RAVENNA -f cd -d 60 -r 44100 -c 8 -t wav /tmp/sink_test.wav > /dev/null 2>&1 &
sleep 10

#starting playback on source
echo "Starting to playback test on source ..."
speaker-test -D plughw:RAVENNA -r 44100 -c 8 -t sine > /dev/null 2>&1 &

while killall -0 arecord 2>/dev/null ; do
  sleep 1
done
killall speaker-test
if [ -f /tmp/sink_test.wav ] ; then
  mv /tmp/sink_test.wav demo/
  echo "Recording to file \"demo/sink_test.wav\" successfull"
else
  echo "Recording failed"
fi

echo "Terminating processes ..."

