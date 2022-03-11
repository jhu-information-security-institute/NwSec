#!/usr/bin/bash
echo 'Removing files'
rm /etc/netplan/101_config.yaml
mv /etc/netplan/backup/* /etc/netplan
rmdir /etc/netplan/backup
