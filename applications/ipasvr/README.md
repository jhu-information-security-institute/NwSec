on host:
```
$ docker build -t tipasvr .
$ docker run -d --name ipasvr --hostname ipa.netsec.isi.jhu.edu --restart=always --privileged --network host --cpus=2 tipasvr:latest
$ sudo firewall-cmd --add-service={freeipa-ldap,freeipa-ldaps,dns,ntp,kerberos}
$ sudo firewall-cmd --runtime-to-permanent
$ docker exec -it ipasvr bash
```

in container:
```
# ipa-server-install --hostname='ipa.netsec.isi.jhu.edu' --domain=netsec.isi.jhu.edu --realm=NETSEC.ISI.JHU.EDU --no-ntp
```
