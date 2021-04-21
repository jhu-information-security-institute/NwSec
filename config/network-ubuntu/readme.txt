* Create Ubuntu server VM as detailed on https://github.com/jhu-information-security-institute/NwSec/wiki/JHUISI-VM
* Create x7 additional network interfaces that are attached to VMnet1
* Copy the Ubuntu server VM to a second version
    * Name the original Ubuntu server 1
    * Name the copy Ubuntu server 2
* Copy etc_netplan_100_config.yaml to /etc/netplan/99_config.yaml on Ubuntu server 1
* Copy etc_netplan_99_config.yaml to /etc/netplan/99_config.yaml on Ubuntu server 2
* Use nmcli to query the ethernet mac addresses of Ubuntu server 1 and Ubuntu server 2
* Update etc_dhcp_dhcpd.conf in the dhcpsvr project based on your ethernet mac addresses from above
* Create docker containers for dnssvr and dhcpsvr on Ubuntu server 2

Boot order:
* Boot Ubuntu server 2
    * Start dnssvr (uses ens38, static ip)
    * Start dhcpsvr (uses ens39, static ip)
    * Configure dhcp assigned addresses by running the following commands
        * # dhclient -v ens40
        * # dhclient -v ens41
        * # dhclient -v ens42
        * # dhclient -v ens43
    * Start other containers
* Boot Ubuntu server 1
    * Start desired containers
