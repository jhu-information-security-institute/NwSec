# Application overview
NFS server

## Server (on RPI4B)
The NFS server on the RPI4B runs in a docker container.   The RPI4B is using Ubuntu Server OS, version 18.04.

## Client (on VM)
The client that communicates with the NFS server is any remote NFS sclient.  

# Runtime environment setup
## Server
1. Disable the firewall on the appropriate port:
`$ sudo ufw allow 2049/tcp`
1. Build the Docker container using: `$ sudo docker build -t tnfssvr .`
1. Start the Docker container using: `$ sudo docker run -d --name nfssvr --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network host --cpus=1 tnfssvr:latest`
1. Log in to the running container using: `$ sudo docker exec -it nfssvr bash`
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
