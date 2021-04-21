#!/usr/bin/bash
# Requires installing bridge-utils: $ sudo apt-get install bridge-utils
# Must be run as root
# e.g., # ./create-bridge.sh -n br0 -d ens38 -g 192.168.25.1/24

while getopts ":hn:d:g:" opt; do
	case ${opt} in
		h )
			echo "Usage:"
			echo "  create-bridge.sh -h		Display this help message."
			echo "  create-bridge.sh -n name -d device -g gateway cidr ip."
			exit 0
			;;
		n )
			name=$OPTARG
			;;
		d )
			device=$OPTARG
			;;
		g )
			gateway=$OPTARG
			;;
		\? )
			echo "Incorrect usage.  For help, run create-bridge.sh -h"
			exit 1
			;;
	esac
done

echo $name
echo $device
echo $gateway

echo "Creating the bridge"
brctl addbr $name
echo "Adding device to the bridge"
brctl addif $name $device
echo "Configuring the bridge gateway cidr ip"
ip addr add $gateway dev $name
echo "Bringing the bridge device up"
ip link set dev $name up
echo "Finished"
