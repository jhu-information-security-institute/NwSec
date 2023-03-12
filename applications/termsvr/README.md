# Application overview
## inetutils-inetd
* Configuration file located in `/etc/inetd.conf`
* Reload config using `$ sudo systemctl daemon-reload`
* Enable service using `$ systemctl enable inetutils-inetd`
* Restart service using `$ systemctl restart inetutils-inetd`
## openssh-server
* Configuration file located in `/etc/ssh/sshd_config`
* Reload config using `$ sudo systemctl daemon-reload`
* Enable service using `$ systemctl enable ssh`
* Restart service using `$ systemctl restart ssh`

## Ubuntu
The terminal server instances on Ubuntu run in a single docker container.

# Runtime environment setup
## Ubuntu
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/master/applications/termsvr/termsvr_UbuntuServerX86-64.sh
    $ chmod +x termsvr_UbuntuServerX86-64.sh
    $ ./termsvr_UbuntuServerX86-64.sh
    ```
1. Build, run, attach to container
    ```
    $ docker build -t ttermsvr .
    $ docker run -d --name termsvr --hostname termsvr.netsec-docker.isi.jhu.edu --add-host termsvr.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network host -p 2323:23 -p 2222:22 --cpus=1 ttermsvr:latest
    $ docker exec -it termsvr bash 
    ```
* Note that the above command remaps port 23 in the docker container to 2323 on the host VM
# Useful websites

