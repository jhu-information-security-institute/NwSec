# Description:
#   This Dockerfile creates a container with a minimal Ubuntu server
# Usage:
#   From this directory, run $ sudo docker build -t tsystemdubuntuserverenv .
# By default, runs as root
FROM ubuntu:18.04

ENV container docker
ENV LC_ALL C
ENV DEBIAN_FRONTEND noninteractive

#enable source package repos
RUN sed -i 's/# deb/deb/g' /etc/apt/sources.list

RUN apt-get update
RUN apt-get upgrade -y
RUN apt-get install apt-utils dpkg-dev net-tools iputils-ping -y
RUN apt-get install build-essential vim help2man -y
RUN apt-get install inetutils-inetd inetutils-telnetd -y

#RUN echo "telnet                  stream  tcp     nowait  root    /usr/local/libexec/telnetd      telnetd" >> /etc/inetd.conf
RUN echo "telnet                  stream  tcp     nowait  root    /usr/sbin/telnetd      telnetd" >> /etc/inetd.conf

RUN apt-get install systemd systemd-sysv -y
#RUN apt-get clean

RUN rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN rm -f /lib/systemd/system/multi-user.target.wants/* \
    /etc/systemd/system/*.wants/* \
    /lib/systemd/system/local-fs.target.wants/* \
    /lib/systemd/system/sockets.target.wants/*udev* \
    /lib/systemd/system/sockets.target.wants/*initctl* \
    /lib/systemd/system/basic.target.wants/* \
    /lib/systemd/system/anaconda.target.wants/* \
    /lib/systemd/system/plymouth* \
    /lib/systemd/system/systemd-update-utmp*

VOLUME [ "/sys/fs/cgroup" ]

# Finished!
RUN echo 'Container is ready, run it using $ sudo docker run -d --name systemdubuntuserverenv --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host tsystemdubuntuserverenv:latest'
RUN echo 'Then attach to it using $ sudo docker exec -it systemdubuntuserverenv bash'

CMD ["/lib/systemd/systemd"]

#https://hub.docker.com/r/jrei/systemd-ubuntu/dockerfile
