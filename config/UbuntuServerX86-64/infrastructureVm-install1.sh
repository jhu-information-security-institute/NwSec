#!/usr/bin/bash
#Must be run as root!
#Backs up /etc/netplan and copies over configuration files into /etc/netplan
echo 'Backup files in /etc/netplan'
mkdir -p /etc/netplan/backup
mv /etc/netplan/*.yaml /etc/netplan/backup
echo 'Copying netplan scripts to /etc/netplan'
cp etc_netplan_99-config.yaml /etc/netplan/99-config.yaml
echo 'Setup network-manager'
mv /usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf  /usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf_orig
touch /usr/lib/NetworkManager/conf.d/10-globally-managed-devices.conf
systemctl restart network-manager
