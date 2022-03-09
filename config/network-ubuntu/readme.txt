* Create Ubuntu server VM as detailed on https://github.com/jhu-information-security-institute/NwSec/wiki/JHUISI-VM
* Create x7 additional network interfaces that are attached to VMnet1 and disable dhcp on VMnet1
* Copy the Ubuntu server VM to a second version
    * Name the original Ubuntu server 1
    * Name the copy Ubuntu server 2
* Copy etc_netplan_101_config.yaml to /etc/netplan/101_config.yaml on Ubuntu server 1
* Copy etc_netplan_99_config.yaml to /etc/netplan/99_config.yaml on Ubuntu server 2
* Install the netplan scripts on Ubuntu server 2 (i.e., run install.sh)
* Ensure that /etc/NetworkManager/NetworkManager.conf has the lines below
[ifupdown]
managed=false
* Use nmcli to query the ethernet mac addresses of Ubuntu server 1 and Ubuntu server 2
* Update etc_dhcp_dhcpd.conf in the dhcpsvr project based on your ethernet mac addresses from above
* Create docker containers for dnssvr and dhcpsvr on Ubuntu server 2
* Copy johntheripper.zip from our nwsec share on mssilab and place it into your Kali VM
    * Alter your attack Dockerfile to point to the johntheripper.zip

Boot order:
* Boot Ubuntu server 2
    * Start dnssvr (uses ens38, static ip)
    * Start dhcpsvr (uses ens39, static ip)
    * Configure dhcp assigned addresses by running the following commands
	* # ~/netplan/warmstart_netplan.sh -c ~/netplan/100_config.yaml
    * Start other containers
* Boot Ubuntu server 1
    * Start desired containers
* Before shutting down Ubuntu server 2, run
    * # ~/netplan/prepshutdown_netplan.sh
    
Notes:
* If you lose dns in your Ubuntu VMs for internet sites, it is likely that your /etc/resolv.conf needs updating
    * Add your router's IP address as an additional nameserver on a new line in /etc/resolv.conf
