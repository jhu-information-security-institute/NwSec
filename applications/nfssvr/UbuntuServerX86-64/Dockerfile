# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and nfs server
# Usage:
#   From this directory, run $ docker build -t tnfssvr .
# By default, runs as root
FROM ubuntu:20.04

#https://grigorkh.medium.com/fix-tzdata-hangs-docker-image-build-cdb52cc3360d
ENV TZ=US/Eastern
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

#avoid question/dialog during apt-get installs
ARG DEBIAN_FRONTEND noninteractive

# Setup container's ENV for systemd
ENV container docker
ENV HOSTNAME nfs.nwsecdocker.jhu.edu

#update
RUN apt-get update

#install utilities
RUN apt-get install apt-utils dpkg-dev debconf debconf-utils -y
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute -y
RUN apt-get install vim acl sudo telnet ssh netcat nfs-common -y

#install dependencies
RUN apt-get install systemd systemd-sysv syslog-ng syslog-ng-core syslog-ng-mod-sql syslog-ng-mod-mongodb -y

#install all the things (nfs server)
RUN apt-get install nfs-kernel-server -y

#create the directory that will be shared
RUN mkdir -p /nfsshare
RUN touch /nfsshare/reubwashere

#update configuration files
COPY etc_exports /etc/exports
COPY etc_fstab /etc/fstab

VOLUME [ "/sys/fs/cgroup" ]
#in docker containers, it is necessary to specify volumes for exported nfs directories
VOLUME [ "/export/nfsshare" ]
VOLUME [ "/nfsshare" ]

CMD ["/lib/systemd/systemd"]

# Finished!
RUN echo $'\n\
* Make sure that you have created the /export/nfsshare and /nfsshare directories on the VM itself prior to running the container \n\    
* Container is ready, run it using $ docker run -d --name nfssvr --hostname nfs.nwsecdocker.jhu.edu --add-host nfs.nwsecdocker.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search nwsecdocker.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro -v /export/nfsshare:/export/nfsshare:rw -v /nvsshare:/nfsshare:rw --network host --cpus="1" tnfssvr:latest \n\
* Attach to it using $ docker exec -it nfssvr bash \n\
* nfs-kernel-server status is available using # systemctl status nfs-kernel-server \n\
* nfs-kernel-server can be stopped and started as well using systemctl'


