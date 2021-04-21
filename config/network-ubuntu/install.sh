#!/usr/bin/bash
mkdir -p ~/netplan
cp etc_netplan_100_config.yaml ~/netplan/100_config.yaml
cp prepshutdown_netplan.sh ~/netplan/.
cp warmstart_netplan.sh ~/netplan/.
