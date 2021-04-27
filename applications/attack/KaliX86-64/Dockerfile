# Description:
#   This runtime environment example Dockerfile creates a container for attacking and demonstrations
# Usage:
#   From this directory, run $ docker build -t tattack .
#   By default, runs as root
# https://docs.docker.com/network/macvlan
# http://fabiorehm.com/blog/2014/09/11/running-gui-apps-with-docker

FROM kalilinux/kali:latest

#avoid question/dialog during apt-get installs
ARG DEBIAN_FRONTEND noninteractive

# Setup container's ENV for systemd
ENV container docker
ENV HOSTNAME attack.nwsecdocker.jhu.edu

#update
RUN apt-get update

#set the root password
#RUN echo -e "root\ntoor" | passwd

#install utilities
RUN apt-get install apt-utils dpkg-dev debconf debconf-utils -y
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute isc-dhcp-common -y
RUN apt-get install vim acl sudo telnet ssh netcat nfs-common -y
RUN apt-get install hping3 -y
RUN apt-get install hydra hydra-gtk ismtp set -y

RUN apt-get install thunderbird firefox-esr sendmail -y

#install dependencies
RUN apt-get install systemd systemd-sysv syslog-ng syslog-ng-core syslog-ng-mod-sql syslog-ng-mod-mongodb -y

RUN useradd -m student
RUN echo 'student:nwsec' | chpasswd
RUN usermod -aG sudo student
RUN echo 'student ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/student

USER student
ENV HOME /home/student

COPY lame.txt /home/student/.
COPY email_common_roots.txt /home/student/.

USER root
VOLUME [ "/sys/fs/cgroup" ]
CMD ["/lib/systemd/systemd"]

# Finished!
RUN echo $'\n\
* Container is ready, prep the bridge network using $ docker network create --driver macvlan --subnet=192.168.25.0/24 -o parent=eth1 macvlan \n\
* run it using $ docker run -d --name attack --hostname attack --add-host attack:127.0.1.1 --dns 192.168.25.10 --dns-search nwsecdocker.jhu.edu --privileged -e DISPLAY=$DISPLAY -v /sys/fs/cgroup:/sys/fs/cgroup:ro -v /tmp/.X11-unix:/tmp/.X11-unix --network macvlan --ip 192.168.25.50 tattack:latest \n\
* Attach to it using $ docker exec -it attack bash \n\
* Change to your user via $ su student \n\
* attack!'


