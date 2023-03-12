# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and telnetd server
# Usage:
#   From this directory, run $ sudo docker build -t tsystemdubuntuserverenv .
# By default, runs as root
FROM ubuntu:18.04

ENV DEBIAN_FRONTEND noninteractive
# Setup ENV for systemd
ENV container docker

#set the root password
RUN echo -e "ubuntu\nubuntu" | passwd

#enable source package repos
RUN sed -i 's/# deb/deb/g' /etc/apt/sources.list
# add a # to recomment this line in /etc/apt/sources.list to avoid a warning (W: Skipping acquire of configured file 'stable/source/Sources' as repository 'https://download.docker.com/linux/ubuntu bionic InRelease' does not seem to provide it (sources.list entry misspelt?)
):
# deb-src [arch=amd64] https://download.docker.com/linux/ubuntu bionic stable

#update and upgrade
RUN apt-get update
RUN apt-get upgrade -y

#install utilities and dependencies
RUN apt-get install apt-utils dpkg-dev net-tools iputils-ping -y
RUN apt-get install sudo build-essential vim help2man autotools-dev autoconf -y
RUN apt-get install inetutils-inetd inetutils-telnetd -y
RUN apt-get install systemd systemd-sysv -y

#hack /etc/pam.d/login to disable pam_securetty
RUN sed -i 's/auth \[success\=ok new_authtok_reqd\=ok ignore\=ignore user_unknown\=bad default\=die\] pam_securetty.so/#auth \[success\=ok new_authtok_reqd\=ok ignore\=ignore user_unknown\=bad default\=die\] pam_securetty.so/g' /etc/pam.d/login

#RUN echo "telnet                  stream  tcp     nowait  root    /usr/local/libexec/telnetd      telnetd" >> /etc/inetd.conf
RUN echo "telnet                  stream  tcp     nowait  root    /usr/sbin/telnetd      telnetd" >> /etc/inetd.conf

VOLUME [ "/sys/fs/cgroup" ]

# Finished!
RUN echo 'Container is ready, run it using $ sudo docker run -d --name systemdubuntuserverenv --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host tsystemdubuntuserverenv:latest'
RUN echo 'Then attach to it using $ sudo docker exec -it systemdubuntuserverenv bash'

CMD ["/lib/systemd/systemd"]

#https://hub.docker.com/r/jrei/systemd-ubuntu/dockerfile
