#!/usr/bin/bash
#Must be run as root!
#Backs up /etc/netplan and copies over configuration files into /etc/netplan
echo 'Backup files in /etc/netplan'
mkdir -p /etc/netplan/backup
mv /etc/netplan/*.yaml /etc/netplan/backup
echo 'Copying netplan configuration files to /etc/netplan'
cp etc_netplan_101-config.yaml /etc/netplan/101-config.yaml
echo 'Disable network-manager'
systemctl stop NetworkManager
systemctl disable NetworkManager
