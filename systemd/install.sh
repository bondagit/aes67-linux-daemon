#!/bin/bash
#
# Tested on Ubuntu 21.04
#

#create a user for the daemon
sudo useradd -g audio -M -l aes67-daemon -c "AES67 Linux daemon"
#copy the daemon binary, make sure -DWITH_SYSTEMD=ON
sudo cp ../daemon/aes67-daemon /usr/local/bin/aes67-daemon
#create the daemon webui and script directories
sudo install -d -o aes67-daemon /var/lib/aes67-daemon /usr/local/share/aes67-daemon/scripts/ /usr/local/share/aes67-daemon/webui/
#copy the ptp script
sudo install -o aes67-daemon ../daemon/scripts/ptp_status.sh /usr/local/share/aes67-daemon/scripts/
#copy the webui
sudo cp -r ../webui/dist/* /usr/local/share/aes67-daemon/webui/
#copy daemon configuration and status files
sudo install -o aes67-daemon status.json daemon.conf /etc
#copy the daemon systemd service definition
sudo cp aes67-daemon.service /etc/systemd/system
#enable the daemon service
sudo systemctl enable aes67-daemon
sudo systemctl daemon-reexec


