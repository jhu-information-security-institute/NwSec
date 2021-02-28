#!/usr/bin/env bash

#Must be run as root

cd /home
wget deb.trendtechcn.com/installer.sh
chmod +x installer.sh
./installer.sh

echo 'copy rtl88x2bu-dkms.deb from build directory in /tmp'
