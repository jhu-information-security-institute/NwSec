* Create Ubuntu server VM as detailed on https://github.com/jhu-information-security-institute/NwSec/wiki/JHUISI-VM
* Create x5 additional network interfaces that are attached to VMnet1 and disable dhcp on VMnet1
* Copy the Ubuntu server VM to a second version
    * Name the original Ubuntu server UbuntuX86-64-target
    * Name the copy Ubuntu server UbuntuX86-64-infrastructure
* Copy etc_netplan_101_config.yaml to /etc/netplan/101_config.yaml on UbuntuX86-64-target
* Copy etc_netplan_99_config.yaml to /etc/netplan/99_config.yaml on UbuntuX86-64-infrastructure
* Install the netplan scripts on UbuntuX86-64-infrastructure (i.e., run install.sh)
* Ensure that /etc/NetworkManager/NetworkManager.conf has the lines below
[ifupdown]
managed=false
* Use nmcli to query the ethernet mac addresses of UbuntuX86-64-target and UbuntuX86-64-infrastructure
* Update etc_dhcp_dhcpd.conf in the dhcpsvr project based on your ethernet mac addresses from above
* Create docker containers for dnssvr and dhcpsvr on UbuntuX86-64-infrastructure
* Copy johntheripper.zip from our nwsec share on mssilab and place it into your Kali VM
    * Alter your attack Dockerfile to point to the johntheripper.zip

Boot order:
* Boot UbuntuX86-64-infrastructure
    * Start dnssvr (uses ens38, static ip) container
    * Start dhcpsvr (uses ens39, static ip) container
    * Configure dhcp assigned addresses by running the following commands
	* # ~/netplan/warmstart_netplan.sh -c ~/netplan/100_config.yaml
    * Start other containers
* Boot UbuntuX86-64-target
    * Start desired containers (e.g., nginxsvr or mailsvr)
* Always shut down UbuntuX86-64-target prior to UbuntuX86-64-infrastructure
* Before shutting down UbuntuX86-64-infrastructure, run
    * # ~/netplan/prepshutdown_netplan.sh
    
Notes:
* If you lose dns in your Ubuntu VMs for internet sites, it is likely that your /etc/resolv.conf needs updating
    * Add your router's IP address as an additional nameserver on a new line in /etc/resolv.conf
