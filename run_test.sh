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

if ! [ -x "$(command -v /usr/sbin/ptp4l)" ]; then
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
sudo sysctl -w net/ipv4/igmp_max_memberships=66

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
if [ -f ./test/sink_test.wav ] ; then
  rm -f sink_test.wav
fi

echo "Starting PTP master ..."
sudo /usr/sbin/ptp4l -i lo -l7 -E -S &

#echo "Starting AES67 daemon ..."
./daemon/aes67-daemon -c ./test/daemon.conf &

#open browser on configuration page
if [ -x "$(command -v xdg-open)" ]; then
  xdg-open "http://127.0.0.1:8080/PTP"
fi

echo "Waiting for PTP slave to sync ..."
sleep 30

#starting recording on sink
echo "Starting to record 240 secs from sink ..."
arecord -M -d 240 -D plughw:RAVENNA -f S24_3LE -r 48000 -c 2 -t wav /tmp/sink_test.wav > /dev/null 2>&1 &
sleep 10

#starting playback on source
echo "Starting to playback test on source ..."
aplay -M -D plughw:RAVENNA  ./test/test.wav > /dev/null 2>&1 &

while killall -0 arecord 2>/dev/null ; do
  sleep 1
done
killall aplay
if [ -f /tmp/sink_test.wav ] ; then
  mv /tmp/sink_test.wav test/
  echo "Recording to file \"test/sink_test.wav\" successfull"
else
  echo "Recording failed"
fi

echo "Terminating processes ..."

