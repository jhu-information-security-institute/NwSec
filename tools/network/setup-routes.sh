#!/usr/bin/bash
# Must be run as root
# e.g., # ./setup-routes.sh -d ens38 -m 192.168.25.0/24
# e.g., # ./setup-routes.sh -d ens39 -m 192.168.25.0/24

while getopts ":hd:m:" opt; do
	case ${opt} in
		h )
			echo "Usage:"
			echo "  setup-routes.sh -h		Display this help message."
			echo "  setup-routes.sh -d device -m subnet cidr."
			exit 0
			;;
		d )
			device=$OPTARG
			;;
		m )
			subnet=$OPTARG
			;;
		\? )
			echo "Incorrect usage.  For help, run setup-route.sh -h"
			exit 1
			;;
	esac
done

ip route del $subnet dev $device
