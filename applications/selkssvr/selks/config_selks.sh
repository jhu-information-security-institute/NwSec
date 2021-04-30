#!/usr/bin/bash
#must be run as sudo

echo "downloading geoipupdate"
curl -L -O https://github.com/maxmind/geoipupdate/releases/download/v4.7.1/geoipupdate_4.7.1_linux_amd64.deb
echo "proceed to install geoipupdate (yes/no)? "
read PROCEED
if [[ $PROCEED == "yes" ]]; then
	dpkg -i geoipupdate_4.7.1_linux_amd64.deb
else
	echo "exiting"
	exit 0
fi

echo "proceed to run selks-first-time-setup (yes/no)? "
read PROCEED
if [[ $PROCEED == "yes" ]]; then
	echo "at first prompt, specify ens34 interface for the SELKS Suricata IDPS threat detection"
	echo "at second prompt, specify 1, for enabling FPC (Full Packet Capture)"
	selks-first-time-setup_stamus
else
	echo "exiting"
	exit 0
fi

echo "proceed to run selks-upgrade_stamus (yes/no)? "
read PROCEED
if [[ $PROCEED == "yes" ]]; then
	echo "when prompted, specify Y to continue install"
	echo "when prompted, specify <Yes> for allowing non-superusers to capture packets"
	echo "when prompted, specify N to keep current versions of configuration files"
	echo "when prompted, specify <Yes> for configuring database for scirius with dbconfig-common"
	selks-upgrade_stamus
else
	echo "exiting"
	exit 0
fi

echo "proceed to run configure elasticsearch, kibana, logstash, and scirius (yes/no)? "
read PROCEED
if [[ $PROCEED == "yes" ]]; then
	#setup SELKS
	echo "stopping elasticsearch, kibana, and logstash"
	systemctl stop logstash kibana elasticsearch
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
	mkdir -p ~/backup/scirius
	mv /etc/scirius/local_settings.py ~/backup/scirius/.
	cp etc_scirius_local_settings.py /etc/scirius/local_settings.py
	
	#reenable SELKS services
	systemctl daemon-reload
	echo "enabling elasticsearch, kibana, and logstash"
	systemctl enable elasticsearch kibana logstash
	echo "starting elasticsearch, kibana, and logstash"
	systemctl start elasticsearch kibana logstash
else
	echo "exiting"
	exit 0
fi

echo "system rebooting"
sleep 3s
shutdown -r now
