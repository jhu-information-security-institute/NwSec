# Setup
* Create Ubuntu server infrastructure VM as described on [Ubuntu-x86-64-VM](https://github.com/jhu-information-security-institute/NwSec/wiki/Ubuntu-x86-64-VM)
* Install Docker in the infrastructure VM as described on [Docker-on-Ubuntu](https://github.com/jhu-information-security-institute/NwSec/wiki/Docker-on-Ubuntu)
* Ensure that your VM has x4 additional virtual network interfaces that are attached to VMnet1 and VMWare's dhcp server is disabled on VMnet1
* Name your VM UbuntuX86-64-infrastructure and create a snapshot
* From your infrastructure VM, download the files into a sandbox directory using
   ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/master/config/infrastructureVm-UbuntuServerX86-64.sh
    $ chmod +x infrastructureVm-UbuntuServerX86-64.sh
    $ ./infrastructureVm-UbuntuServerX86-64.sh
    ```
* Create Ubuntu server DHCP Docker container as described on [dhcpsvr](https://github.com/jhu-information-security-institute/NwSec/tree/master/applications/dhcpsvr)
* Create Ubuntu server DNS Docker container as described on [dnssvr](https://github.com/jhu-information-security-institute/NwSec/tree/master/applications/dnssvr)
* Change into the downloaded `infrastructureVm/config/UbuntuServerX86-64` directory
* Run the installer1 with sudo using: `$ sudo ./infrastructureVm-install1.sh`
* Run the installer2 using: `$ ./infrastructureVm-install2.sh`
* Edit nameserver IP address and domain name in nameserver section of `ens33` portion within `/etc/netplan/99-config.yaml` to values for your internet router
    * Run `$ ip route` in a terminal and use whatever ip address the "default via XXX.XXX.XXX.XXX" is
    * Ensure that the device names `ens??` match with those created in your vm
* Run `$ sudo netplan apply`
* Ensure that the device names `ens??` within `~/netplan/100-config.yaml` match with those created in your vm
* Ensure that /etc/NetworkManager/NetworkManager.conf has the lines below
    ```
    [ifupdown]
    managed=false
    ```
* Start Ubuntu server DHCP Docker container as described on [dhcpsvr](https://github.com/jhu-information-security-institute/NwSec/tree/master/applications/dhcpsvr)
* Start Ubuntu server DNS Docker container as described on [dnssvr](https://github.com/jhu-information-security-institute/NwSec/tree/master/applications/dnssvr)    
* Use nmcli to query the ethernet mac addresses the VMNet1 virtual network adapters on UbuntuX86-64-infrastructure
* Update `/etc/dhcp/dhcpd.conf` in the dhcpsvr project based on your ethernet mac addresses from above
* Reload and restart isc-dhcp-server in your container
* Shutdown UbuntuServerX86-64-infrastructure and take a snapshot

# Startup
* UbuntuX86-64-infrastructure should always boot first (before UbuntuX86-64-target) because it provides DHCP and DNS to VMNet1
* After booting UbuntuX86-64-infrastructure
    * Start dnssvr (uses ens38, static ip) container using `$ sudo docker start dnssvr`
    * Start dhcpsvr (uses ens39, static ip) container using `$ sudo docker start dhcpsvr`
    * Configure dhcp assigned addresses by running `$ sudo ~/netplan/warmstart-netplan.sh -c ~/netplan/100-config.yaml`
    * Start other containers (e.g., suricata or kerberos)
        * start suricata (uses ens41, static ip) container using `$ sudo docker start suricata`

# Shutdown
* Always shut down UbuntuX86-64-target prior to shutdown of UbuntuX86-64-infrastructure
* Before shutting down UbuntuX86-64-infrastructure, run `$ sudo ~/netplan/prepshutdown-netplan.sh -c 100-config.yaml`
    
# Notes
* If you lose dns for internet sites, it is likely that your `/etc/resolv.conf` needs updating
    * In your VM, add your internet router's IP address as an additional nameserver by creating a new line in `/etc/resolv.conf`
    ```
    nameserver <IPADDRESSOFINTERNETROUTER>
    nameserver 8.8.8.8
    ```
* If your container build fails due to dns not working, follow the 'Specify DNS servers for Docker' [here](https://docs.docker.com/engine/install/troubleshoot)
* https://netplan.readthedocs.io/en/latest/netplan-yaml
