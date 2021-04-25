# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and wireshark
# Usage:
#   From this directory, run $ docker build -t twireshark .
#   By default, runs as root
#   https://www.wireshark.org/

FROM ubuntu:20.04

#https://grigorkh.medium.com/fix-tzdata-hangs-docker-image-build-cdb52cc3360d
ENV TZ=US/Eastern
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

#avoid question/dialog during apt-get installs
ARG DEBIAN_FRONTEND noninteractive

# Setup container's ENV for systemd
ENV container wireshark
ENV HOSTNAME wireshark.nwsecdocker.jhu.edu

#update
RUN apt-get update

#install utilities
RUN apt-get install apt-utils dpkg-dev debconf -y
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute -y
RUN apt-get install vim acl sudo telnet ssh netcat -y

RUN echo 'Install dependencies'

#install all the things (wireshark)
RUN echo "wireshark-common wireshark-common/install-setuid boolean true" | sudo debconf-set-selections
RUN apt-get install wireshark -y

RUN useradd -m rjohn177
RUN usermod -aG sudo rjohn177
RUN usermod -aG wireshark rjohn177
RUN echo 'rjohn177 ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/rjohn177

USER rjohn177
ENV HOME /home/rjohn177

RUN echo $'\n\
* Container is ready, run it using $ docker run --name wireshark --hostname wireshark.nwsecdocker.jhu.edu --add-host wireshark.nwsecdocker.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search wireshark.jhu.edu --privileged -e DISPLAY=$DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix --network host --cpus="1" -ti twireshark:latest bash \n\
* Start wireshark using $ sudo -u rjohn177 wireshark'
