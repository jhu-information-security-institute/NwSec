on host:
```
$ docker run -d --name ipasvr --hostname ipa.netsec.isi.jhu.edu --privileged --network host --cpus=2 tipasvr:latest
$ sudo firewall-cmd --add-service={freeipa-ldap,freeipa-ldaps,dns,ntp}
$ sudo firewall-cmd --runtime-to-permanent
$ sudo yum install krb5-workstation krb5-libs
```

in container:
```
# ipa-server-install --hostname='ipa.netsec.isi.jhu.edu' --domain=netsec.isi.jhu.edu --realm=netsec.isi.jhu.edu --ds-password=<PASSWORD> --admin-password=<PASSWORD> --no-ntp
```
