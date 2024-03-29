# Application overview
NFS and SSH servers

## Server (on UbuntuServerx86-64-target VM)
The VM is using Ubuntu Server OS, version 20.04.  It is designed to run within a Docker container in our [UbuntuServerx86-64-target VM](https://github.com/jhu-information-security-institute/NwSec/blob/master/config/UbuntuServerX86-64/targetVm-README.md).

## Client
The client that communicates with the NFS and SSH servers are common clients for the same.

## Server (on VM)
1. Add entries in DHCP and DNS servers for nfssvr-plus-termsvr.netsec-docker.isi.jhu.edu that setup an interface in your UbuntuServerx86-64-target VM to be assigned 192.168.25.123
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/master/applications/nfssvr%2Btermsvr/nfssvr_plus_termsvr_UbuntuServerX86-64.sh
    $ chmod +x nfssvr_plus_termsvr_UbuntuServerX86-64.sh
    $ ./nfssvr_plus_termsvr_UbuntuServerX86-64.sh
    ```
1. Build, run, attach to container (the tar command -h dereferences symlinks into files)
    ```
    $ tar -czh . | docker build -t tnfssvr_plus_termsvr -
    $ docker run -d --name nfssvr_plus_termsvr --hostname nas --add-host nas.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network host --cpus=1 tnfssvr_plus_termsvr:latest
    $ docker exec -it nfssvr_plus_termsvr bash 
    ```
1. See the notes at the following link for configuration of SSH (ssh service): `https://github.com/jhu-information-security-institute/NwSec/blob/master/applications/termsvr/README.md`
1. See the notes at the following link for configuration of NFS (nfs-kernel-server service): `https://github.com/jhu-information-security-institute/NwSec/blob/master/applications/nfssvr/README.md`

# Notes
* Services include: `nfs-kernel-server`, `inetutils-inetd`, and `ssh`
* Reload the configuration by running `$ sudo systemctl daemon-reload`
* Restart the server using: `$ sudo systemctl restart <SERVICE>`
* Check the server status (there should be no errors) using: `$ sudo systemctl status <SERVICE>`
* View the server log: `$ sudo journalctl -u <SERVICE>`

# Useful websites
Todo
