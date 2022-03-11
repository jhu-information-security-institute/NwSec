#!/usr/bin/bash
echo 'Removing files'
rm /etc/netplan/99_config.yaml
rm /etc/netplan/100_config.yaml
rm /etc/netplan
rm /etc/netplan
mv /etc/netplan/backup/* /etc/netplan
rmdir /etc/netplan/backup
