## Ubuntu
The client instances on Ubuntu runs in a single docker container.

# Runtime environment setup
## Ubuntu
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/master/applications/client/client_UbuntuX86-64.sh
    $ chmod +x client_UbuntuX86-64.sh
    $ ./client_UbuntuX86-64.sh
    ```
1. Build, run, attach to container
    ```
    $ docker build -t tclient .
    $ docker run -d --name client --hostname jhedid-client.netsec.isi.jhu.edu --add-host jhedid-client.netsec.isi.jhu.edu:127.0.1.1 --dns 172.16.0.10 --dns-search netsec.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network bridge --cpus=1 tclient:latest
    $ docker exec -it client bash 
    ```
# Useful websites

