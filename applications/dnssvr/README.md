# Application overview
Versatile, classic, complete name server software

BIND 9 has evolved to be a very flexible, full-featured DNS system. Whatever your application is, BIND 9 probably has the required features. As the first, oldest, and most commonly deployed solution, there are more network engineers who are already familiar with BIND 9 than with any other system.

# Runtime environment setup
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/master/applications/dnssvr/dnssvr-UbuntuServerX86-64.sh
    $ chmod +x dnssvr-UbuntuServerX86-64.sh
    $ ./dnssvr-UbuntuServerX86-64.sh
    ```
1. Build, run, attach to container
    ```
    $ docker build -t tdnssvr .
    $ docker run -d --name dnssvr --hostname ns.netsec-docker.isi.jhu.edu --add-host ns.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network host --cpus=1 tdnssvr:latest
    $ sudo docker exec -it dnssvr bash 
    ```
1. Enable the server using: `$ sudo systemctl enable bind9`

## Notes
* Restart the server using: `# systemctl restart bind9`
* Check the server status (there should be no errors) using: `# systemctl status bind9`
* View the server log: `# journalctl -u bind9`
* Configure the server by editing `/etc/bind/zones/db.25.168.192` and `/etc/bind/zones/db.netsec-docker.isi.jhu.edu`

# Useful websites
* https://www.isc.org/bind
* https://netbeez.net/blog/linux-how-to-resolve-a-host-and-test-dns-servers/
* https://linuxconfig.org/how-to-view-and-clear-bind-dns-server-s-cache-on-linux
* https://bind9.readthedocs.io/en/latest/introduction.html
