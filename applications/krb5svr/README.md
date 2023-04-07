# Kerberos setup

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
    $ docker build -t tauthsvr .
    $ docker run -d --name authsvr --hostname auth.netsec-docker.isi.jhu.edu --add-host auth.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus=1 tauthsvr:latest
    $ docker exec -it authsvr bash 
    ```

## Setup KDC container (auth.netsec-docker.isi.jhu.edu)
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
* Note: you need dns entry for your KDC for this to work (edit /etc/hosts for nfs.netsec-docker.isi.jhu.edu)

## Create principal users
* Authenticate as kerberos admin principal using: `$ kinit ubuntu/admin`
* Example 1, create a principal for the nfs server host (need to use the correct dns name for it)
  <pre><code>
  $ kadmin -q "addprinc -randkey nfs/nfs.netsec-docker.isi.jhu.edu"
  </code></pre>
* Example 2, create a principal for the client host (need to use the correct dns name for it)
  <pre><code>
  $ kadmin -q "addprinc -randkey client/client.netsec-docker.isi.jhu.edu"
  </code></pre>
* Example 3, create a principal for a user
  <pre><code>
  $ kadmin -q "addprinc -randkey ubuntu/student"
  </code></pre>  

## Configure client hosts
* Ensure network and DNS are setup properly on the client
* Install krb5-user on the client
  <pre><code>
  $ sudo apt-get install krb5-user
  </code></pre>  
* Example 1, setup NFS client
    * From the NFS client, authenticate as kerberos admin principal using: `$ kinit ubuntu/admin`
    * Add the client server principal to the keytab-file on the nfs server client (need to use the correct dns name for your termsvr+nfs host)
    <pre><code>
    $ kadmin -p ubuntu/admin -q "ktadd nfs/nfs.netsec-docker.isi.jhu.edu"
    </code></pre>
    * Query the keytab settings (should see the principal listed above) using: `$ ktutil`

# Useful links
* https://ubuntu.com/server/docs/service-kerberos
* https://help.ubuntu.com/community/NFSv4Howto
