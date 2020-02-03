# Note:
# currently ptp4l disables IP_MULTICAST_LOOP, so sockets on the same host cannot receive ptp4l traffic.
# Only exception is for loopback network interface.
#
sudo ptp4l -i $1 -m -l7 -E -S
