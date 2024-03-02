## Ubuntu
The client instances on Ubuntu runs in a single docker container.

# Runtime environment setup
## Ubuntu
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/main/applications/client/client_UbuntuX86-64.sh
    $ chmod +x client_UbuntuX86-64.sh
    $ ./client_UbuntuX86-64.sh
    ```
1. Build, run, attach to container
    ```
    $ docker build -t tclient .
    $ docker run -d --name client --hostname client.netsec-docker.isi.jhu.edu --add-host client.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network host --cpus=1 tclient:latest
    $ docker exec -it client bash 
    ```
# Useful websites
