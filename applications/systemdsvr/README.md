# Runtime environment setup
## Ubuntu
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/master/applications/systemdsvr/systemdsvr-UbuntuServerX86-64.sh
    $ chmod +x systemdsvr_UbuntuServerX86-64.sh
    $ ./systemdsvr_UbuntuServerX86-64.sh
    ```
1. Build, run, attach to container
    ```
    $ docker build -t tsystemdsvr .
    $ docker run -d --name systemdsvr --hostname systemdsvr.netsec-docker.isi.jhu.edu --add-host systemdsvr.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network host --cpus=1
    $ docker exec -it systemdsvr bash 
    ```
