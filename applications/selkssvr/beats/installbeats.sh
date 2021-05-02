#!/usr/bin/bash
# e.g., # ./installbeats.sh -c websvr -a
# Note: purge remove package via dpkg is '-P <PKGNAME>'

FILEBEAT=0
HEARTBEAT=0
METRICBEAT=0
PACKETBEAT=0
CONTAINER="NULL"

while getopts ":hc:afHpm" opt; do
	case ${opt} in
		h )
			echo "Usage:"
			echo "  installbeats.sh -h			Displays this help message."
			echo "  installbeats.sh -c container -f	Installs filebeat to container."
			echo "  installbeats.sh -c container -H	Installs heartbeat to container."
			echo "  installbeats.sh -c container -m	Installs metricbeat to container."
			echo "  installbeats.sh -c container -p	Installs packetbeat to container."
			exit 0
			;;
		c )
			CONTAINER=$OPTARG
			;;
		f )
			FILEBEAT=1
			;;	
		H )
			HEARTBEAT=1
			;;	
		m )
			METRICBEAT=1
			;;	
		p )
			PACKETBEAT=1
			;;				
		\? )
			echo "Incorrect usage.  For help, run installbeats.sh -h"
			exit 1
			;;
	esac
done

if [[ $CONTAINER == "NULL" ]]; then
	echo "Usage:"
	echo "  installbeats.sh -h			Displays this help message."
	echo "  installbeats.sh -c container -f	Installs filebeat to container."
	echo "  installbeats.sh -c container -H	Installs heartbeat to container."
	echo "  installbeats.sh -c container -m	Installs metricbeat to container."
	echo "  installbeats.sh -c container -p	Installs packetbeat to container."
	exit 0
fi

if [[ $FILEBEAT -eq 1 ]]; then
	echo "Installing filebeat to $CONTAINER"
	docker cp filebeat-7.12.1-amd64.deb $CONTAINER:/root
	docker exec -it $CONTAINER bash -c "DEBIAN_FRONTEND=noninteractive dpkg -i /root/filebeat-7.12.1-amd64.deb"
	docker cp etc_filebeat_filebeat.yml $CONTAINER:/etc/filebeat/filebeat.yml
	docker exec -it $CONTAINER bash -c "chmod go-w /etc/filebeat/filebeat.yml" 
	docker exec -it $CONTAINER bash -c "systemctl enable filebeat"
	docker exec -it $CONTAINER bash -c "systemctl start filebeat"
fi

if [[ $HEARTBEAT -eq 1 ]]; then
	echo "Installing heartbeat to $CONTAINER"
	docker cp heartbeat-7.12.1-amd64.deb $CONTAINER:/root
	docker exec -it $CONTAINER bash -c "DEBIAN_FRONTEND=noninteractive dpkg -i /root/heartbeat-7.12.1-amd64.deb"
	docker cp etc_heartbeat_heartbeat.yml $CONTAINER:/etc/heartbeat/heartbeat.yml
	docker exec -it $CONTAINER bash -c "chmod go-w /etc/heartbeat/heartbeat.yml" 
	docker exec -it $CONTAINER bash -c "systemctl enable heartbeat-elastic"
	docker exec -it $CONTAINER bash -c "systemctl start heartbeat-elastic"	
fi

if [[ $METRICBEAT -eq 1 ]]; then
	echo "Installing metricbeat to $CONTAINER"
	docker cp metricbeat-7.12.1-amd64.deb $CONTAINER:/root
	docker exec -it $CONTAINER bash -c "DEBIAN_FRONTEND=noninteractive dpkg -i /root/metricbeat-7.12.1-amd64.deb"
	docker cp etc_metricbeat_metricbeat.yml $CONTAINER:/etc/metricbeat/metricbeat.yml
	docker exec -it $CONTAINER bash -c "chmod go-w /etc/metricbeat/metricbeat.yml" 
		docker exec -it $CONTAINER bash -c "systemctl enable metricbeat"
	docker exec -it $CONTAINER bash -c "systemctl start metricbeat"	
fi

if [[ $PACKETBEAT -eq 1 ]]; then
	docker cp packetbeat-7.12.1-amd64.deb $CONTAINER:/root
	docker exec -it $CONTAINER bash -c "DEBIAN_FRONTEND=noninteractive dpkg -i /root/packetbeat-7.12.1-amd64.deb"
	docker cp etc_packetbeat_packetbeat.yml $CONTAINER:/etc/packetbeat/packetbeat.yml
	docker exec -it $CONTAINER bash -c "chmod go-w /etc/packetbeat/packetbeat.yml" 
	docker exec -it $CONTAINER bash -c "systemctl enable packetbeat"
	docker exec -it $CONTAINER bash -c "systemctl start packetbeat"	
fi
