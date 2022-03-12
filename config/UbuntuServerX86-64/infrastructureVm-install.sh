#!/usr/bin/bash
#Must be run as root!
#Backs up /etc/netplan and copies over configuration files and scripts into ~/netplan
echo 'Backup files in /etc/netplan'
mkdir -p /etc/netplan/backup
mv /etc/netplan/* /etc/netplan/backup
echo 'Copying netplan scripts and configuration files to ~/netplan'
mkdir -p ~/netplan
cp etc_netplan_99-config.yaml ~/netplan/99-config.yaml
cp etc_netplan_100-config.yaml ~/netplan/100-config.yaml
cp warmstart-netplan.sh ~/netplan/warmstart-netplan.sh
cp prepshutdown-netplan.sh ~/netplan/prepshutdown-netplan.sh
systemctl disable NetworkManager
