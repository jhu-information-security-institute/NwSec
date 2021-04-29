#!/usr/bin/bash
#must be run as sudo
curl -L -O https://github.com/maxmind/geoipupdate/releases/download/v4.7.1/geoipupdate_4.7.1_linux_amd64.deb
dpkg -i geoipupdate_4.7.1_linux_amd64.deb

#setup NetworkManager
systemctl stop NetworkManager
mkdir -p ~/backup/NetworkManager
mv /etc/NetworkManager/system-connections/* ~/backup/NetworkManager/.
cp etc_NetworkManager_system-connections_Wired_connection_1.nmconnection /etc/NetworkManager/system-connections/Wired_connection_1.nmconnection
cp etc_NetworkManager_system-connections_Wired_connection_2.nmconnection /etc/NetworkManager/system-connections/Wired_connection_2.nmconnection
mv /etc/NetworkManager/NetworkManager.conf ~/backup/NetworkManager/.
cp etc_NetworkManager_NetworkManager.conf /etc/NetworkManager/NetworkManager.conf

#disable ipv6
mkdir -p ~/backup/default
mv /etc/default/grub ~/backup/default/.
cp etc_default_grub /etc/default/grub
cp etc_sysctl.d_70-disable-ipv6.conf /etc/sysctl.d/70-disable-ipv6.conf
update-grub

#setup resolved
systemctl stop systemd-resolved
mkdir -p ~/backup/systemd
mv /etc/systemd/resolved.conf ~/backup/systemd/.
cp etc_systemd_resolved.conf /etc/systemd/resolved.conf

#setup SELKS
systemctl stop elasticsearch kibana logstash
mkdir -p ~/backup/elasticsearch
mv /etc/elasticsearch/elasticsearch.yml ~/backup/elasticsearch/.
mkdir -p ~/backup/kibana
mv /etc/kibana/kibana.yml ~/backup/kibana/.
mkdir -p ~/backup/logstash
mv /etc/logstash/logstash.yml ~/backup/logstash/.
mv /etc/logstash/conf.d/logstash.conf ~/backup/logstash/.
cp etc_elasticsearch_elasticsearch.yml /etc/elasticsearch/elasticsearch.yml
cp etc_kibana_kibana.yml /etc/kibana/kibana.yml
cp etc_logstash_logstash.yml /etc/logstash/logstash.yml
cp etc_logstash_conf.d_logstash.conf /etc/logstash/conf.d/logstash.conf
cd /usr/share/logstash && bin/logstash --setup --path.settings /etc/logstash -e

#reenable networking related services
systemctl daemon-reload
systemctl enable NetworkManager systemd-resolved
systemctl start NetworkManager systemd-resolved

#reenable SELKS services
systemctl enable elasticsearch kibana logstash
systemctl start elasticsearch kibana logstash

