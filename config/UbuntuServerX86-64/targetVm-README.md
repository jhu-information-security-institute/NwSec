# Initial setup
* Create Ubuntu server target VM as described on [Ubuntu-x86-64-VM](https://github.com/jhu-information-security-institute/NwSec/wiki/Ubuntu-x86-64-VM)
* Install Docker in the target VM as described on [Docker-on-Ubuntu](https://github.com/jhu-information-security-institute/NwSec/wiki/Docker-on-Ubuntu)
* Ensure that your VM has x4 additional virtual network interfaces that are attached to VMnet1 and VMWare's dhcp server is disabled on VMnet1
* Name your VM UbuntuServerX86-64-target and create a snapshot
* From your target VM, download the files into a sandbox directory using
   ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/master/config/targetVm-UbuntuServerX86-64.sh
    $ chmod +x targetVm-UbuntuServerX86-64.sh
    $ ./targetVm-UbuntuServerX86-64.sh
    ```
* Change into the downloaded `targetVm/config/UbuntuServerX86-64` directory
* Run the installer using: `$ sudo ./targetVm-install.sh`
* Edit nameserver IP address and domain name in nameserver section of `ens33` portion within `/etc/netplan/101-config.yaml` to values for your internet router
  * Ensure that the device names `ens??` within `/etc/netplan/101-config.yaml` match with those created in your vm
* Ensure that /etc/NetworkManager/NetworkManager.conf has the lines below
    ```
    [ifupdown]
    managed=false
    ```
# Final setup
* Use nmcli to query the ethernet mac addresses the VMNet1 virtual network adapters on UbuntuX86-64-target
* Update `/etc/dhcp/dhcpd.conf` in the dhcpsvr project based on your ethernet mac addresses from above
* Reload and restart isc-server-dhcp in your container
* Shutdown UbuntuServerX86-64-target and take a snapshot

# Startup
* Always boot UbuntuX86-64-target after booting UbuntuServerX86-64-infrastructure VM
* Start desired containers (e.g., nginxsvr or mailsvr)

# Shutdown
* Always shut down UbuntuX86-64-target prior to UbuntuX86-64-infrastructure
    
# Notes
* If you lose dns for internet sites, it is likely that your `/etc/resolv.conf` needs updating
    * In your VM, add your internet router's IP address as an additional nameserver by creating a new line in `/etc/resolv.conf`
    ```
    nameserver <IPADDRESSOFINTERNETROUTER>
    nameserver 8.8.8.8
    ```
