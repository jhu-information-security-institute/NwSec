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
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute -y
RUN apt-get install vim acl sudo telnet ssh netcat nfs-common -y
RUN apt-get install hping3 -y
RUN apt-get install hydra hydra-gtk ismtp set -y

RUN apt-get install thunderbird firefox-esr sendmail -y

RUN useradd -m rjohn177
RUN usermod -aG sudo rjohn177
RUN echo 'rjohn177 ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/rjohn177

USER rjohn177
ENV HOME /home/rjohn177

RUN cp ~/Downloads/johntheripper.zip /home/rjohn177/.
RUN cp ../email_common_roots.txt /home/rjohn177/.
RUN mkdir /mnt/nfs

# Finished!
RUN echo $'\n\
* Container is ready, prep the bridge network using $ docker network create --driver macvlan --subnet=192.168.25.0/24 -o parent=eth1 macvlan \n\
* run it using $ docker run --name attack --hostname attack --add-host attack:127.0.1.1 --dns 192.168.25.10 --dns-search nwsecdocker.jhu.edu --privileged -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix --network macvlan --ip 192.168.25.50 -ti tattack:latest bash \n\
* attack!'


