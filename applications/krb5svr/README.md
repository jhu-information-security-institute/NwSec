# Kerberos setup
* Components
    * Key Distribution Center (KDC)
    * Admin server
    * Client nodes

172.16.0.30 KDC

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

# Useful links
* https://ubuntu.com/server/docs/service-kerberos
* https://help.ubuntu.com/community/NFSv4Howto
