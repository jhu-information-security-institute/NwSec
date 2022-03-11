#!/usr/bin/bash
echo 'Backup files in /etc/netplan'
mkdir -p /etc/netplan/backup
mv /etc/netplan/* /etc/netplan/backup
echo 'Copying netplan scripts and configuration files to /etc/netplan'
cp etc_netplan_99_config.yaml /etc/netplan/99_config.yaml
cp etc_netplan_100_config.yaml /etc/netplan/100_config.yaml
cp warmstart_netplan.sh ~/netplan/.
cp prepshutdown_netplan.sh ~/netplan/.
