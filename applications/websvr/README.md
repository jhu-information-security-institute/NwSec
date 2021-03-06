# Application overview
NGINX is a free, open-source, high-performance HTTP server and reverse proxy, as well as an IMAP/POP3 proxy server. NGINX is known for its high performance, stability, rich feature set, simple configuration, and low resource consumption.

Our application sets up the nginx web server on the RPI4B in a docker container and allows local interfacing with the server using a browser or telnet from the same LAN.  Remote access to the server over http is possible, but ports must be enabled through any firewalls (e.g., ports 80 and 81).

## Server (on RPI4B)
The web server on the RPI4B runs in a docker container.  The RPI4B is using Ubuntu Server OS, version 18.04.  Nginx handles all incoming HTTP requests that come in over the enabled ports and generates appropriate HTTP responses for them.

## Client (on VM)
The client that communicates with the web server is any remote browser.  One can also directly interface by manually applying direct http commands using telnet over port 81.  E.g., see the command/response sequence below (note: commands have no indentation and  responses are indented).
<pre><code>
$ # telnet 192.168.50.46 81
Trying 192.168.50.46...
Connected to 192.168.50.46.
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
## Server (on RPI4B)
1. Build the Docker container using: `$ sudo docker build -t twebsvr .`
1. Start the Docker container using: `$ sudo docker run -d --name websvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host twebsvr:latest`
1. Log in to the running container using: `$ sudo docker exec -it websvr bash`
1. Restart the server using: `# systemctl restart nginx`
1. Check the server status (there should be no errors) using: `# systemctl status nginx`

# Useful websites
* https://www.nginx.com/resources/wiki
* https://ubuntu.com/tutorials/install-and-configure-nginx
