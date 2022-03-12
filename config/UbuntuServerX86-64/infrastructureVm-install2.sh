#!/usr/bin/bash
#copies over configuration files and scripts into ~/netplan
echo 'Copying netplan scripts and configuration files to ~/netplan'
mkdir -p ~/netplan
cp etc_netplan_100-config.yaml ~/netplan/100-config.yaml
cp warmstart-netplan.sh ~/netplan/warmstart-netplan.sh
cp prepshutdown-netplan.sh ~/netplan/prepshutdown-netplan.sh
