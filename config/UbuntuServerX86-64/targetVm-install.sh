#!/usr/bin/bash
#Must be run as root!
#Backs up /etc/netplan and copies over configuration files into /etc/netplan
echo 'Backup files in /etc/netplan'
mkdir -p /etc/netplan/backup
mv /etc/netplan/* /etc/netplan/backup
echo 'Copying netplan configuration files to /etc/netplan'
cp etc_netplan_101-config.yaml /etc/netplan/101-config.yaml
echo 'Setup network-manager'
sudo mv /usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf  /usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf_orig
sudo touch /usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf
sudo systemctl restart network-manager
