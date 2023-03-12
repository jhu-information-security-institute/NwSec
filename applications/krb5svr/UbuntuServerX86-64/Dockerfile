# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and kerberos kdc and admin servers
# Usage:
#   From this directory, run $ docker build -t tauthsvr .
#   By default, runs as root
#   https://ubuntu.com/server/docs/service-kerberos

FROM ubuntu:20.04

#https://grigorkh.medium.com/fix-tzdata-hangs-docker-image-build-cdb52cc3360d
ENV TZ=US/Eastern
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

#avoid question/dialog during apt-get installs
ARG DEBIAN_FRONTEND noninteractive

# Setup container's ENV for systemd
ENV container docker

#update
RUN apt-get update

#install utilities
RUN apt-get install apt-utils dpkg-dev debconf debconf-utils -y
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute isc-dhcp-common -y
RUN apt-get install vim acl sudo telnet ssh netcat nfs-common -y

#install dependencies for systemd and syslog
RUN apt-get install systemd systemd-sysv syslog-ng syslog-ng-core syslog-ng-mod-sql syslog-ng-mod-mongodb -y

#start systemd
CMD ["/usr/lib/systemd/systemd", "--system"]

#install all the things (kdc and admin server)
RUN echo "krb5-config krb5-config/default_realm string NETSEC-DOCKER.ISI.JHU.EDU" | sudo debconf-set-selections
RUN echo "krb5-config krb5-config/kerberos_servers string auth.netsec-docker.isi.jhu.edu" | sudo debconf-set-selections
RUN echo "krb5-config krb5-config/admin_server string auth.netsec-docker.isi.jhu.edu" | sudo debconf-set-selections
RUN apt-get install krb5-kdc krb5-admin-server -y

COPY etc_krb5.conf /etc/krb5.conf
COPY etc_krb5kdc_kdc.conf /etc/krb5kdc/kdc.conf
COPY etc_krb5kdc_kadm5.acl /etc/krb5kdc/kadm5.acl

# Finished!
RUN echo $'\n\
* Container is ready, run it using $ docker run -d --name authsvr --hostname auth.netsec-docker.isi.jhu.edu --add-host auth.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus=1 tauthsvr:latest \n\
* Attach to it using $ docker exec -it authsvr bash \n\
* Build database using # kdb5_util create -r NETSEC-DOCKER.ISI.JHU.EDU -s \n\
* Restart the servers # systemctl restart krb5-admin-server krb5-kdc \n\
* krb5-kdc status is available using # systemctl status krb5-kdc \n\
* krb5-admin-server status is available using # systemctl status krb5-admin-server \n\
* krb5-kdc and krb5-admin-server can be stopped and started as well using systemctl \n\
* Log in to kadmin using # kadmin.local \n\
    * create your administrative pricipal user using: kadmin.local: addprinc student/admin \n\
        * set the password when prompted \n\
    * quit kadmin using: kadmin.local: quit \n\
* Restart the server to reload the new ACL using: $ sudo systemctl restart krb5-admin-server krb5-kdc \n\
* Test the principle user using # kinit student/admin \n\
* Check if you can see the ticket assigned # klist'
