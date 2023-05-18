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
echo 'Creating test file ...' >&2
if ! ./createtest $1 $2 $3 $4 ; then
  echo 'Usage run_test.sh sample_format sample_rate channels duration' >&2
  echo '  sample_format can be one of S16_LE, S24_3LE, S32_LE' >&2
  echo '  sample_rate can be one of 44100, 48000, 96000, 192000, 384000' >&2
  echo '  channels can be one of 2, 4, 6, up to 64' >&2
  echo '  duration is in the range 1 to 10 minutes' >&2
  exit 1
else
  echo 'test file created' >&2
fi
cd ..

SAMPLE_FORMAT=$1
SAMPLE_RATE=$2
CHANNELS=$3
DURATION=$4
SEC=$((DURATION*60))

if [ $SAMPLE_FORMAT == "S16_LE" ]; then
  CODEC="L16"
elif [ $SAMPLE_FORMAT == "S24_3LE" ]; then
  CODEC="L24"
elif [ $SAMPLE_FORMAT == "S32_LE" ] ; then
  CODEC="AM824"
fi

if [ $SAMPLE_RATE == "44100" ]; then
  PTIME="1.08843537415"
elif  [ $SAMPLE_RATE == "48000" ]; then
  PTIME="1"
elif  [ $SAMPLE_RATE == "96000" ]; then
  PTIME="0.5"
elif  [ $SAMPLE_RATE == "192000" ]; then
  PTIME="0.25"
elif  [ $SAMPLE_RATE == "384000" ]; then
  PTIME="0.125"
fi

SOURCE=$(cat <<-END
{
    "id": ID,
    "enabled": true,
    "name": "ALSA Source ID",
    "io": "Audio Device",
    "max_samples_per_packet": 48,
    "codec": "CODEC",
    "address": "",
    "ttl": 15,
    "payload_type": 98,
    "dscp": 34,
    "refclk_ptp_traceable": false,
    "map": [ MS, ME ]
}
END
)

SOURCES='{ "sources": [ '
for (( ch=0; ch<$(( $CHANNELS / 2 )); ch++ ))
do
   CSOURCE=$(echo $SOURCE | sed "s/ID/$ch/g;s/CODEC/$CODEC/g;s/MS/$(( 2*$ch ))/g;s/ME/$(( 2*$ch + 1 ))/g;")
   SOURCES+=$CSOURCE
   if (( ch != ($(( $CHANNELS / 2 )) - 1) )); then
     SOURCES+=","
   fi
done
SOURCES+=" ],"

SINK=$(cat <<-END
  {
    "id": ID,
    "name": "ALSA Sink ID",
    "io": "Audio Device",
    "use_sdp": true,
    "source": "http://127.0.0.1:8080/api/source/sdp/0",
    "sdp": "v=0\no=- 657152 657153 IN IP4 127.0.0.1\ns=ALSA Source ID\nc=IN IP4 239.1.0.ADDR/15\nt=0 0\na=clock-domain:PTPv2 0\nm=audio 5004 RTP/AVP 98\nc=IN IP4 239.1.0.ADDR/15\na=rtpmap:98 CODEC/SR/2\na=sync-time:0\na=framecount:48\na=ptime:PTIME\na=mediaclk:direct=0\na=ts-refclk:ptp=IEEE1588-2008:00-00-00-00-00-00-00-00:0\na=recvonly\n",
    "delay": 576,
    "ignore_refclk_gmid": true,
    "map": [ MS, ME ]
  }
END
)

SINKS='"sinks": [ '
for (( ch=0; ch<$(( $CHANNELS / 2 )); ch++ ))
do
   CSINK=$(echo $SINK | sed "s/ID/$ch/g;s/ADDR/$(( $ch+1 ))/g;s/CODEC/$CODEC/g;s/SR/$SAMPLE_RATE/g;s/PTIME/$PTIME/g;s/MS/$(( 2*$ch ))/g;s/ME/$(( 2*$ch + 1 ))/g;")
   SINKS+=$CSINK
   if (( ch != ($(( $CHANNELS / 2 )) - 1) )); then
     SINKS+=","
   fi
done
SINKS+=" ] }"

echo 'Creating configuration files ..' >&2
sed 's/48000/'"$SAMPLE_RATE"'/g;s/status.json/status_.json/g;' test/daemon.conf > test/daemon_.conf
#sed 's/48000/'"$SAMPLE_RATE"'/g;s/L24/'"$CODEC"'/g;s/ptime:1/ptime:'"$PTIME"'/g;' test/status.json > test/status_.json
echo $SOURCES > test/status_.json
echo $SINKS >> test/status_.json

trap cleanup EXIT

#configure system parms
sudo sysctl -w net/ipv4/igmp_max_memberships=66
sudo sysctl -w kernel/perf_cpu_time_max_percent=0
sudo sysctl -w kernel/sched_rt_runtime_us=1000000

if [ -x /usr/bin/pulseaudio ]; then
  #stop pulseaudio, this seems to open/close ALSA continuosly
  echo autospawn = no > $HOME/.config/pulse/client.conf
  pulseaudio --kill >/dev/null 2>&1
  rm $HOME/.config/pulse/client.conf
  #disable pulseaudio
  systemctl --user stop pulseaudio.socket > /dev/null 2>&1
  systemctl --user stop pulseaudio.service > /dev/null 2>&1
fi

#uninstall kernel module
if sudo lsmod | grep MergingRavennaALSA > /dev/null 2>&1 ; then
  sudo rmmod MergingRavennaALSA
fi

#install kernel module
sudo insmod 3rdparty/ravenna-alsa-lkm/driver/MergingRavennaALSA.ko

cleanup
if [ -f ./test/sink_test.raw ] ; then
  rm -f ./test/sink_test.raw
fi

echo "Starting PTP master ..."
sudo /usr/sbin/ptp4l -i lo -l7 -E -S &

echo "Starting AES67 daemon ..."
./daemon/aes67-daemon -c ./test/daemon_.conf &

echo "Waiting for PTP slave to sync ..."
sleep 30

#starting record on sink
echo "Starting to record $SEC sec from sink ..."
arecord -M -d $SEC -D plughw:RAVENNA -f $SAMPLE_FORMAT -r $SAMPLE_RATE -c $CHANNELS -t raw /tmp/sink_test.raw  &
sleep 10

#starting playback on source
echo "Starting to playback test on source ..."
aplay -M -D plughw:RAVENNA -f $SAMPLE_FORMAT -r $SAMPLE_RATE -c $CHANNELS ./test/test.raw > /dev/null 2>&1 &

while killall -0 arecord 2>/dev/null ; do
  sleep 1
done
while killall -0 aplay 2>/dev/null ; do
  sleep 1
done
if [ -f /tmp/sink_test.raw ] ; then
  mv /tmp/sink_test.raw test/
  echo "Recording to file \"test/sink_test.raw\" successfull"
else
  echo "Recording failed"
fi

echo "Test result:"
cd test
./check $SAMPLE_FORMAT $CHANNELS
cd ..

echo "Terminating processes ..."
