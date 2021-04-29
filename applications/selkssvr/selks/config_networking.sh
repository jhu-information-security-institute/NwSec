#!/usr/bin/bash
#must be run as sudo

echo "proceed to install configure networking (yes/no)? "
read PROCEED
if [[ $PROCEED == "yes" ]]; then
	#setup NetworkManager
	systemctl stop NetworkManager
	mkdir -p ~/backup/NetworkManager
	mv /etc/NetworkManager/system-connections/* ~/backup/NetworkManager/.
	cp etc_NetworkManager_system-connections_Wired_connection_1.nmconnection /etc/NetworkManager/system-connections/Wired_connection_1.nmconnection
	cp etc_NetworkManager_system-connections_Wired_connection_2.nmconnection /etc/NetworkManager/system-connections/Wired_connection_2.nmconnection
	mv /etc/NetworkManager/NetworkManager.conf ~/backup/NetworkManager/.
	cp etc_NetworkManager_NetworkManager.conf /etc/NetworkManager/NetworkManager.conf
	
	#disable ipv6
	mkdir -p ~/backup/default
	mv /etc/default/grub ~/backup/default/.
	cp etc_default_grub /etc/default/grub
	cp etc_sysctl.d_70-disable-ipv6.conf /etc/sysctl.d/70-disable-ipv6.conf
	update-grub
	
	#setup resolved
	systemctl stop systemd-resolved
	mkdir -p ~/backup/systemd
	mv /etc/systemd/resolved.conf ~/backup/systemd/.
	cp etc_systemd_resolved.conf /etc/systemd/resolved.conf
	
	#reenable networking related services
	systemctl daemon-reload
	systemctl enable NetworkManager systemd-resolved
	systemctl start NetworkManager systemd-resolved
else
	echo "exiting"
	exit 0
fi

echo "system rebooting"
sleep 3s
shutdown -r now
