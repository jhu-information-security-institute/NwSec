#!/usr/bin/bash
# Must be run as root
# e.g., # ./setup_user.sh -u <USER> -H <HOME>

USER="NULL"
HOME="NULL"

while getopts ":hu:H:" opt; do
        case ${opt} in
                h )
                        echo "Usage:"
                        echo "  setup_user.sh -h         	Displays this help message."
                        echo "  setup_user.sh -u <USER> -H <HOME>   Finishes setup of <USER>."
                        exit 0
                        ;;
                u )
                        USER=$OPTARG
                        ;;
                H )
                        HOME=$OPTARG
                        ;;
                \? )
                        echo "Incorrect usage.  For help, run prepshutdown_netplan.sh -h"
                        exit 1
                        ;;
        esac
done

if [ $USER == "NULL" ] || [ $HOME == "NULL" ]; then
    echo "Usage:"
    echo "  setup_user.sh -h         	Displays this help message."
    echo "  setup_user.sh -u <USER> -H <HOME>   Finishes setup of <USER>."
    exit 0
fi

cp -rT /etc/skel $HOME
chown -R $USER $HOME
chgrp -R $USER $HOME
