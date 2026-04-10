# Identity and Access Management with FreeIPA
## Prerequisites
* DNS must be working on all hosts
* NTP must be working on all hosts
* Centos VM that is named iam.netsec-docker.isi.jhu.edu

# NTP setup with Chrony
## NTP server
* Install chrony on iam.netsec-docker.isi.jhu.edu with: `# apt-get install chrony`
* Allow hosts on netsec.isi.jhu.edu to reach chrony by adding the following to `/etc/chrony.conf`
```
allow 192.168.25.0/24 
```
## NTP client
