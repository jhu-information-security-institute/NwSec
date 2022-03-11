#!/usr/bin/bash
#Must be run as root!
#Backs up /etc/netplan and copies over configuration files into /etc/netplan
echo 'Backup files in /etc/netplan'
mkdir -p /etc/netplan/backup
mv /etc/netplan/* /etc/netplan/backup
echo 'Copying netplan scripts and configuration files to /etc/netplan'
cp etc_netplan_100_config.yaml /etc/netplan/100_config.yaml
cp etc_netplan_101_config.yaml /etc/netplan/101_config.yaml
