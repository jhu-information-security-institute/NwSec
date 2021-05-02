# e.g., # ./installevebox.sh -c suricata -a
# Note: purge remove package via dpkg is '-P <PKGNAME>'

CONTAINER="NULL"

while getopts ":hc:" opt; do
	case ${opt} in
		h )
			echo "Usage:"
			echo "  installevebox.sh -h			Displays this help message."
			echo "  installevebox.sh -c container 	Installs evebox to container."
			exit 0
			;;
		c )
			CONTAINER=$OPTARG
			;;
		\? )
			echo "Incorrect usage.  For help, run installevebox.sh -h"
			exit 1
			;;
	esac
done

echo "Installing evebox to $CONTAINER"
docker cp evebox_0.13.1_amd64.deb $CONTAINER:/root
docker exec -it $CONTAINER bash -c "DEBIAN_FRONTEND=noninteractive dpkg -i /root/evebox_0.13.1_amd64.deb"
docker cp etc_default_evebox $CONTAINER:/etc/default/evebox
docker exec -it $CONTAINER bash -c "systemctl daemon-reload"
docker exec -it $CONTAINER bash -c "systemctl restart evebox"