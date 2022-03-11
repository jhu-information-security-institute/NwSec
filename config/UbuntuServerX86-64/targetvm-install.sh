#!/usr/bin/bash
echo 'Making directoris in ~/netplan'
mkdir -p ~/netplan
mkdir -p ~/netplan/backup
echo 'Backing up current configuration'
mv /etc/netplan/* ~/netplan/backup
echo 'Copying netplan scripts and configuration files to ~/netplan'
cp etc_netplan_100_config.yaml ~/netplan/100_config.yaml
cp prepshutdown_netplan.sh ~/netplan/.
cp warmstart_netplan.sh ~/netplan/.
