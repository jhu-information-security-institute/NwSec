# Application overview
NFS server

## Server
The NFS server runs in a docker container.   It is using Ubuntu Server OS, version 22.04.

## Client (on VM)
The client that communicates with the NFS server is any remote NFS sclient.  

# Runtime environment setup
## Server
1. Disable the firewall on the appropriate port:
    `$ sudo ufw allow 2049/tcp`
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/main/applications/nfssvr/nfssvr_UbuntuServerX86-64.sh
    $ chmod +x nfssvr_UbuntuServerX86.sh
    $ ./nfssvr_UbuntuServerX86.sh
    ```
1. Build, run, attach to container
    ```
    $ docker build -t tnfssvr .
    $ docker run -d --name nfssvr --hostname nas.netsec-docker.isi.jhu.edu --add-host attack:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged -e DISPLAY=$DISPLAY --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock -v /tmp/.X11-unix:/tmp/.X11-unix:rw -v /etc/group:/etc/group:ro -v /etc/passwd:/etc/passwd:ro -v /etc/shadow:/etc/shadow:ro -v /home/$USER/.Xauthority:/home/$USER/.Xauthority:rw --network host tnfssvr:latest
    $ docker exec -it nfssvr bash 
    ```
1. From inside the docker session:
    * Ensure `/etc/hosts.allow` contains
    ```
    ALL:ALL
    ```
    * Bind mount the exported directory to the shared directory: `# mount --bind /nfsshare /export/nfsshare`
    * Edit `/etc/fstab` and add the line below to make the bind mount persist over reboots:
    <pre><code>
    /nfsshare    /export/nfsshare   none    bind  0  0
    </code></pre>
    * Edit `/etc/exports` and add the line below to create the share:
    <pre><code>
    /export/nfsshare	192.168.25.0/24(rw,nohide,insecure,no_subtree_check,no_root_squash,async)
    </code></pre>
    * Export the shares: `# exportfs -a`
    * Restart the nfs server: `# systemctl start nfs-kernel-server`
    * Make a file to see from the client: `# touch /nfsshare/hello`

## Client (e.g., Kali VM)
1. Make a directory for the mount: `$ sudo mkdir /mnt/nfs`
1. Mount the nfs share by using the following command (make sure to replace the ip address with your server's ip address): `$ sudo mount -t nfs -o proto=tcp,port=2049 192.168.25.100:/export/nfsshare /mnt/nfs`

# Useful websites
* https://help.ubuntu.com/community/SettingUpNFSHowTo
* https://ubuntu.com/server/docs/service-nfs
