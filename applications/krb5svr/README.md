# Kerberos setup
* Components
    * Key Distribution Center (KDC)
    * Admin server
    * Client nodes

192.168.25.101 KDC (auth.netsec-docker.isi.jhu.edu)

* to install use: `$ sudo apt-get install krb5-kdc krb5-admin-server`
* to purge everything use: `$ sudo apt purge -y krb5-kdc krb5-admin-server krb5-config krb5-locales krb5-user krb5.conf`
* create a kerberos realm and set the database master password: `$ sudo krb5_newrealm`
* create your adminstrative principal user: `$ sudo kadmin.local`
  <pre><code>
  $ sudo kadmin.local
  Authenticating as principal root/admin@EXAMPLE.COM with password.
  kadmin.local: addprinc ubuntu/admin
  WARNING: no policy specified for ubuntu/admin@EXAMPLE.COM; defaulting to no policy
  Enter password for principal "ubuntu/admin@EXAMPLE.COM":
  Re-enter password for principal "ubuntu/admin@EXAMPLE.COM":
  Principal "ubuntu/admin@EXAMPLE.COM" created.
  kadmin.local: quit
  </code></pre>
* Setup ACL for the admin user by editing `/etc/krb5kdc/kadm5.acl` and uncommenting the line below:
  <pre><code>
  */admin *
  </code></pre>
* Restart the server to reload the new ACL using: `$ sudo systemctl restart krb5-admin-server.service`
* Note: you need dns entry for your KDC for this to work (possibly you might be able to edit /etc/hosts if on same machine)
* Test the principal user using: `$ kinit ubuntu/admin`

# Runtime environment setup
## Ubuntu
1. Download files to build container
    ```
    $ wget https://raw.githubusercontent.com/jhu-information-security-institute/NwSec/master/applications/krb5svr/krb5svr_UbuntuServerX86-64.sh
    $ chmod +x krb5svr_UbuntuServerX86-64.sh
    $ ./krb5svr_UbuntuServerX86-64.sh
    ```
1. Build, run, attach to container
    ```
    $ docker build -t ttermsvr .
    $ docker run -d --name termsvr --hostname jhedid-termsvr.netsec.isi.jhu.edu --add-host jhedid-termsvr.netsec.isi.jhu.edu:127.0.1.1 --dns 172.16.0.10 --dns-search netsec.isi.jhu.edu --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network bridge --cpus=1 ttermsvr:latest
    $ docker exec -it termsvr bash 
    ```

# Useful links
* https://ubuntu.com/server/docs/service-kerberos
* https://help.ubuntu.com/community/NFSv4Howto
