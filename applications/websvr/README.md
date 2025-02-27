# Application overview
NGINX is a free, open-source, high-performance HTTP server and reverse proxy, as well as an IMAP/POP3 proxy server. NGINX is known for its high performance, stability, rich feature set, simple configuration, and low resource consumption.

The web server runs in a Docker container and allows local interfacing with the server using a browser or telnet from the same LAN.  Nginx handles all incoming HTTP requests that come in over the enabled ports and generates appropriate HTTP responses for them.  Remote access to the NGINX server over http is possible, but ports must be enabled through any firewalls (e.g., ports 80 and 81).

## Server (on RPI4B)
The RPI4B is using Ubuntu Server OS, version 18.04.

## Server (on UbuntuServerx86-64-target VM)
The VM is using Ubuntu Server OS, version 20.04.  It is designed to run within a Docker container in our [UbuntuServerx86-64-target VM](https://github.com/jhu-information-security-institute/NwSec/blob/master/config/UbuntuServerX86-64/targetVm-README.md).

## Client
The client that communicates with the web server is any remote browser.  One can also directly interface by manually applying direct http commands using telnet over port 8080.  E.g., see the command/response sequence below (note: commands have no indentation and  responses are indented).
<pre><code>
$ # telnet web.netsec-docker.isi.jhu.edu 8080
Trying 192.168.25.121...
Connected to 192.168.25.121.
Escape character is '^]'.
GET /
    &lt!doctype html&gt
    &lthtml&gt
    &lthead&gt
        &ltmeta charset="utf-8"&gt
        &lttitle&gtHello, Students!&lt/title&gt
    &lt/head&gt
    &ltbody&gt
        &lth1&gtHello, EN.650.624 Network Security Students!&lt/h1&gt
        &ltp&gtWe have just configured our Nginx web server on Ubuntu Server!&lt/p&gt
    &lt/body&gt
    &lt/html&gt
Connection closed by foreign host.
</code></pre>

# Runtime environment setup
## Server (on VM)
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/master/applications/websvr/websvr-UbuntuServerX86-64.sh
    $ chmod +x websvr-UbuntuServerX86-64.sh
    $ ./websvr-UbuntuServerX86-64.sh
    ```
1. Build, run, attach to container
    ```
    $ docker build -t twebsvr .
    $ docker run -d --name websvr --hostname web.netsec-docker.isi.jhu.edu --add-host web.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network bridge -p 8080:80 --cpus=1 twebsvr:latest
    $ docker exec -it websvr bash 
    ```
1. Enable the server using: `$ sudo systemctl enable nginx`
1. Ensure that you have at least one server entry located in `/etc/nginx/sites-available`
1. Ensure that your server entries have symbolic links located in `/etc/nginx/sites-available`
1. Reload the configuration by running `$ sudo systemctl daemon-reload`
1. Restart the server by running `$ sudo systemctl start nginx`
1. Page should be viewable in a browser at the following url: `http://192.168.25.120:81/`

## Server (on RPI4B)
1. Build the Docker container using: `$ sudo docker build -t twebsvr .`
1. Start the Docker container using: `$ sudo docker run -d --name websvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host twebsvr:latest`
1. Log in to the running container using: `$ sudo docker exec -it websvr bash`
1. Restart the server using: `# systemctl restart nginx`
1. Check the server status (there should be no errors) using: `# systemctl status nginx`

# Notes
* Restart the server using: `$ sudo systemctl restart nginx`
* Check the server status (there should be no errors) using: `$ sudo systemctl status nginx`
* View the server log: `$ sudo journalctl -u nginx`
* Configure the server by editing server entries located in `/etc/nginx/sites-available`
* Enable sites by adding symbolic links in `/etc/nginx/sites-available` that link back to server entry counterparts in `/etc/nginx/sites-available`

# Useful websites
* https://www.nginx.com
* https://www.nginx.com/resources/wiki
* https://ubuntu.com/tutorials/install-and-configure-nginx
* https://docs.suricata.io/en/latest/rules/header-keywords.html
