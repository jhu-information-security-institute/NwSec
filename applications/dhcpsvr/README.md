# Application overview
ISC DHCP offers a complete open source solution for implementing DHCP servers, relay agents, and clients. ISC DHCP supports both IPv4 and IPv6, and is suitable for use in high-volume and high-reliability applications.

# Runtime environment setup
## Server
1. Build the Docker container using: `$ docker build -t tdhcpsvr .`
1. Start the Docker container using: `$ docker run -d --name dhcpsvr --hostname dhcp.netsec-docker.isi.jhu.edu --add-host dhcp.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus=1 tdhcpsvr:latest`
1. Log in to the running container using: `$ sudo docker exec -it dhcpsvr bash`

## Notes
* Restart the server using: `# systemctl restart isc-dhcp-server`
* Check the server status (there should be no errors) using: `# systemctl status isc-dhcp-server`
* View the server log: `# journalctl -u isc-dhcp-server`
* Configure the server by editing `/etc/dhcp/dhcpd.conf`

# Useful websites
* https://www.isc.org/dhcp
* https://linux.die.net/man/5/dhcpd-options
* https://www.iana.org/assignments/bootp-dhcp-parameters/bootp-dhcp-parameters.xhtml
