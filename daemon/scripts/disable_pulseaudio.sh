#!/bin/bash

echo autospawn = no > $HOME/.config/pulse/client.conf
pulseaudio --kill
rm $HOME/.config/pulse/client.conf

systemctl --user stop pulseaudio.socket > /dev/null 2>&1
systemctl --user stop pulseaudio.service > /dev/null 2>&1


