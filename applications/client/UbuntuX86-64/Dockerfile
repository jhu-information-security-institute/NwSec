# Description:
#   This runtime environment example Dockerfile creates a container with an Ubuntu client
# Usage:
#   From this directory, run $ docker build -t tclient .
#   By default, runs as root
# https://www.gnu.org/software/inetutils/
# https://ubuntu.com/server/docs/service-openssh

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
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute openssh-server isc-dhcp-common -y
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
CMD [ "/lib/systemd/systemd", "log-level=info", "unit=sysinit.target" ]

#create a user student with password nwsec
RUN useradd -m student
RUN echo 'student:nwsec' | chpasswd

# Finished!
RUN echo $'\n\
* Container is ready, run it using $ docker run -d --name client --hostname jhedid-client.netsec.isi.jhu.edu --add-host jhedid-client.netsec.isi.jhu.edu:127.0.1.1 --dns 172.16.0.10 --dns-search netsec.isi.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus=1 --cpus=1 tclient:latest \n\
* Attach to it using $ docker exec -it client bash'
