#!/usr/bin/bash
# Must be run as root
# e.g., # ./prepshutdown-netplan.sh -c 100-config.yaml

while getopts ":hc:" opt; do
        case ${opt} in
                h )
                        echo "Usage:"
                        echo "  prepshutdown-netplan.sh -h         	Displays this help message."
                        echo "  prepshutdown-netplan.sh -c configfile   Removes configfile from /etc/netplan."
                        exit 0
                        ;;
                c )
                        configfile=$OPTARG
                        ;;
                \? )
                        echo "Incorrect usage.  For help, run prepshutdown-netplan.sh -h"
                        exit 1
                        ;;
        esac
done

rm /etc/netplan/$configfile
