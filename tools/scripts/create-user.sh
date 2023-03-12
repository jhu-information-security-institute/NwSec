#!/usr/bin/bash
# Must be run as root
# e.g., # ./create-user.sh -u dummy -p dummy

while getopts ":hu:p:" opt; do
	case ${opt} in
		h )
			echo "Usage:"
			echo "  create-user.sh -h				Display this help message."
			echo "  create-user.sh -u username -p password		Create a user with username and password in sudo group."
			exit 0
			;;
		u ) # process option u
			username=$OPTARG
			;;
		p ) # process option p
			password=$OPTARG
			;;
		\? ) echo "Incorrect usage.  For help, run create-user.sh -h"
			exit 1
		;;
	esac
done

echo $username
echo $password

useradd -m $username
echo $(echo -e "${password}\n${password}" | passwd $username)
usermod -aG sudo $username
usermod -s /bin/bash $username