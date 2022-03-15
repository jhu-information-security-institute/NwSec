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
    $ docker run -d --name termsvr --hostname jhedid-termsvr.netsec.isi.jhu.edu --add-host jhedid-termsvr.netsec.isi.jhu.edu:127.0.1.1 --dns 172.16.0.10 --dns-search netsec.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network bridge --cpus=1 ttermsvr:latest \n\
    $ docker exec -it termsvr bash 
    ```
# Useful websites

