# Description:
#   This runtime environment example Dockerfile creates a container with a tar pit
# Usage:
#   From this directory, run $ docker build -t ttarpit .
#   By default, runs as root
# https://github.com/Snawoot/http-tarpit
# https://github.com/skeeto/endlessh

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

#start systemd
CMD ["/usr/lib/systemd/systemd", "--system"]

RUN apt-get install endlessh python3-pip -y
RUN pip3 install http-tarpit

# Finished!
RUN echo $'\n\
* Container is ready, run it using $ docker run -d --name tar --hostname tar.nwsecdocker.jhu.edu --add-host tar.nwsecdocker.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search nwsecdocker.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus=1 ttarpit:latest \n\
* Attach to it using $ docker exec -it tar bash \n\
* Start the http-tarpit using # http-tarpit \n\
* Start the endlessh tarpit using # endlessh -p <PORT>'
