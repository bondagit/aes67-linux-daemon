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

function usage {
  echo 'Usage run_latenvy_test.sh sample_format sample_rate channels duration' >&2
  echo '  sample_format can be one of S16_LE, S24_3LE, S32_LE' >&2
  echo '  sample_rate can be one of 44100, 48000, 96000' >&2
  echo '  channels can be one of 1, 2, 4' >&2
  echo '  duration in seconds' >&2
  exit 1
}


if [ "$#" -ne 4 ]; then
  usage 
fi

if ! [ -x "$(command -v /usr/sbin/ptp4l)" ]; then
  echo 'Error: ptp4l is not installed.' >&2
  exit 1
fi

if ! [ -x "$(command -v arecord)" ]; then
  echo 'Error: arecord is not installed.' >&2
  exit 1
fi

if ! [ -x "$(command -v aplay)" ]; then
  echo 'Error: aplay is not installed.' >&2
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

cd test
echo 'Compiling tools ...' >&2
make
cd ..

SAMPLE_FORMAT=$1
SAMPLE_RATE=$2
CHANNELS=$3
DURATION=$4

if [ $SAMPLE_FORMAT == "S16_LE" ]; then
  CODEC="L16"
elif [ $SAMPLE_FORMAT == "S24_3LE" ]; then
  CODEC="L24"
elif [ $SAMPLE_FORMAT == "S32_LE" ] ; then
  CODEC="AM824"
else
  usage
fi

if [ $SAMPLE_RATE == "44100" ]; then
  PTIME="1.08843537415"
elif [ $SAMPLE_RATE == "48000" ]; then
  PTIME="1"
elif [ $SAMPLE_RATE == "96000" ]; then
  PTIME="0.5"
else
  usage
fi

MAP="[ "
for (( ch=0; ch<$CHANNELS; ch++ ))
do
   MAP+=$ch
   if (( ch != ($CHANNELS - 1) )); then
     MAP+=","
   fi
done
MAP+=" ]"

echo 'Creating configuration files ..' >&2
sed 's/48000/'"$SAMPLE_RATE"'/g;s/status.json/status_.json/g;' test/daemon.conf > test/daemon_.conf
sed 's/\/2/\/'"$CHANNELS"'/g;s/48000/'"$SAMPLE_RATE"'/g;s/L24/'"$CODEC"'/g;s/ptime:1/ptime:'"$PTIME"'/;s/\[ 0, 1 \]/'"$MAP"'/g' test/status.json > test/status_.json

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

#uninstall kernel module
if sudo lsmod | grep MergingRavennaALSA > /dev/null 2>&1 ; then
  sudo rmmod MergingRavennaALSA
fi

#install kernel module
sudo insmod 3rdparty/ravenna-alsa-lkm/driver/MergingRavennaALSA.ko

cleanup

echo "Starting PTP master ..."
sudo /usr/sbin/ptp4l -i lo -l7 -E -S &

echo "Starting AES67 daemon ..."
./daemon/aes67-daemon -c ./test/daemon_.conf &

echo "Waiting for PTP slave to sync ..."
sleep 30

echo "Running 10 secs latency test"
cd test
./latency -P hw:RAVENNA -C hw:RAVENNA -f $SAMPLE_FORMAT -r $SAMPLE_RATE -c $CHANNELS -M 128 -m 128 -s $DURATION
cd ..

echo "Terminating processes ..."
