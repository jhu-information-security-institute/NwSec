#!/usr/bin/bash
echo 'Making directory ~/netplan'
mkdir -p ~/netplan
echo 'Copying netplan scripts and configuration files to ~/netplan'
cp etc_netplan_100_config.yaml ~/netplan/100_config.yaml
cp prepshutdown_netplan.sh ~/netplan/.
cp warmstart_netplan.sh ~/netplan/.
