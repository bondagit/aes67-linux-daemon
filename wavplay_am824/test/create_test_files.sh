#!/bin/bash
sox -V -r 48000 -n -b 16 -c 2 48k_2ch_16_tones.wav synth 30 sin 1000 sin 100 vol -10dB
sox -V -r 48000 -n -b 24 -c 2 48k_2ch_24_tones.wav synth 30 sin 1000 sin 100 vol -10dB
sox -V -r 44100 -n -b 16 -c 2 44k1_2ch_16_tones.wav synth 30 sin 1000 sin 100 vol -10dB
sox -V -r 48000 -n -b 32 -e floating-point -c 2 48k_2ch_float_tones.wav synth 30 sin 1000 sin 100 vol -10dB
ddp_encoder -a7 -i48k_2ch_24_tones.wav -md1 -dn31 -+d640 -o48k_2ch_24_tones.ac3
# The change from 24bit to 16bit below is not a typo. As a result of the encoding process the bitstream is 16bit
# although the encoded audio was in 24bit format
smpte -i48k_2ch_24_tones.ac3 -o48k_2ch_16_tones.ac3.wav
rm -f 48k_2ch_24_tones.ac3