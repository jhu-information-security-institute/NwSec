#!/usr/bin/bash
# Must be run as root
# e.g., # ./warmstart_netplan.sh -c etc_netplan_100_config.yaml

while getopts ":hc:" opt; do
	case ${opt} in
		h )
			echo "Usage:"
			echo "  warmstart_netplan.sh -h		Displays this help message."
			echo "  setup-routes.sh -c configfile	Copies configfile to /etc/netplan and applies it."
			exit 0
			;;
		c )
			configfile=$OPTARG
			;;
		\? )
			echo "Incorrect usage.  For help, run warmstart_netplan.sh -h"
			exit 1
			;;
	esac
done

cp $configfile /etc/netplan/.
netplan apply
