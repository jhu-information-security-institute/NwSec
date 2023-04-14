# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and a caching only dns server
# Usage:
#   From this directory, run $ docker build -t tdnssvr .
#   By default, runs as root
#   https://www.isc.org/bind

FROM ubuntu:22.04

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

VOLUME [ "/tmp", "/run", "/run/lock" ]
# Remove unnecessary units
RUN rm -f /lib/systemd/system/multi-user.target.wants/* \
  /etc/systemd/system/*.wants/* \
  /lib/systemd/system/local-fs.target.wants/* \
  /lib/systemd/system/sockets.target.wants/*udev* \
  /lib/systemd/system/sockets.target.wants/*initctl* \
  /lib/systemd/system/sysinit.target.wants/systemd-tmpfiles-setup* \
  /lib/systemd/system/systemd-update-utmp*
#start systemd
CMD [ "/lib/systemd/systemd", "log-level=info", "unit=sysinit.target" ]

#install all the things (web server)
RUN apt-get install bind9 -y

RUN mkdir -p /etc/bind/zones
COPY etc_default_named /etc/default/named
COPY etc_bind_named.conf.options /etc/bind/named.conf.options
COPY etc_bind_named.conf.local /etc/bind/named.conf.local
COPY etc_bind_zones_db.netsec-docker.isi.jhu.edu /etc/bind/zones/db.netsec-docker.isi.jhu.edu
COPY etc_bind_zones_db.25.168.192 /etc/bind/zones/db.25.168.192
#COPY etc_resolv.conf /etc/resolv.conf

# Finished!
RUN echo $'\n\
* Container is ready, run it using $ docker run -d --name dnssvr --hostname ns.netsec-docker.isi.jhu.edu --add-host ns.netsec-docker.isi.jhu.edu:127.0.1.1 --privileged --security-opt seccomp=unconfined --cgroup-parent=docker.slice --cgroupns private --tmpfs /tmp --tmpfs /run --tmpfs /run/lock --network host --cpus=1 tdnssvr:latest \n\
* Attach to it using $ docker exec -it dnssvr bash \n\
* bind9 status is available using # systemctl status bind9 \n\
* bind9 can be stopped and started as well using systemctl'
