#!/usr/bin/bash
echo 'Removing files'
rm -Rf ~/netplan
rm /etc/netplan/99-config.yaml
rm /etc/netplan/100-config.yaml
mv /etc/netplan/backup/* /etc/netplan
rmdir /etc/netplan/backup
