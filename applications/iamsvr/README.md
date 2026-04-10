# Identity and Access Management with FreeIPA
## Prerequisites
* DNS must be working on all hosts and their hostnames should be FQDN (term.netsec-docker.isi.jhu.edu).
* NTP must be working on all hosts.  See the chrony instructions below.
* Centos VM that is named iam.netsec-docker.isi.jhu.edu

# NTP setup with Chrony
## NTP server
* Install chrony on iam.netsec-docker.isi.jhu.edu with: `# apt-get install chrony`
* Allow hosts on netsec.isi.jhu.edu to reach chrony by adding the following to `/etc/chrony.conf`
```
allow 192.168.25.0/24 
```
## NTP client
Uses chrony to sync with ntp.   
* Set the time zone to America/New York 
* Install chrony: `$ sudo apt-get install chrony`
* Add the following line to /etc/chrony/chrony.conf 
```
pool iam.netsec-docker.isi.jhu.edu iburst 
```
* Then, enable, reload, start chrony service and query status with: 
```
$ sudo systemctl daemon-reload 
$ sudo systemctl enable chrony 
$ sudo systemctl restart chrony 
```
* Query status on the client by running: `$ chronyc sources`

# FreeIPA server setup on CentOS VM
* Install the dependencies: `# yum install krb5-workstation krb5-libs freeipa-server`
* Run the installer:
```
# ipa-server-install --hostname=iam.netsec-docker.isi.jhu.edu --domain=netsec-docker.isi.jhu.edu --realm=NETSEC-DOCKER.ISI.JHU.EDU
```
* Update the firewall and open the ports
```
# firewall-cmd --add-service={freeipa-ldap,freeipa-ldaps,ntp,kerberos,http,https,ssh} 
# firewall-cmd --runtime-to-permanent
```
* Confirm it is working by logging in to Kerberos: `$ kinit admin`
* Then run: `$ ipa user-find admin`
* You should get a response with `1.User matched` that is followed with details on that user

# FreeIPA client setup
* Set your hostname using: `# hostname <NAME>.netsec.isi.jhu.edu`
* Edit `/etc/hostname` and put `<NAME>.netsec.isi.jhu.edu` in it
* Add an entry for your client in `/etc/hosts`:
```
<NETSEC_IPADDRESS> <NETSEC_HOST>.netsec.isi.jhu.edu <NETSEC_HOST>
```
* Install freeipa-client package: `$ sudo apt-get install freeipa-client`
* Work through the prompts (todo, add detail)
* Run the client installer (time needs to be synchronized and network connected or this will fail!) 
```
# ipa-client-install
```
* After installing freeipa-client, sssd is configured to activate certain services in `/etc/sssd/sssd.conf` [source](https://groups.google.com/g/linux.debian.bugs.dist/c/6i8L12q3uYw?pli=1): 
... 
[sssd] 
services = nss, pam, ssh, sudo 
...
*     Disable the sssd-*.socket socket-activated systemd services (note, these are enabled by default) to eliminate receiving errors in the journald log when booting 
```
# systemctl disable sssd-nss.socket 
# systemctl disable sssd-pam.socket 
# systemctl disable sssd-pam-priv.socket 
# systemctl disable sssd-sudo.socket 
# systemctl disable sssd-ssh.socket 
# systemctl disable sssd-autofs.socket 
```

# FreeIPA client reinstall notes
```
# ipa-client-install --uninstall 
# ipa-client-install 
# systemctl disable sssd-nss.socket 
# systemctl disable sssd-pam.socket 
# systemctl disable sssd-pam-priv.socket 
# systemctl disable sssd-sudo.socket 
# systemctl disable sssd-ssh.socket 
```
