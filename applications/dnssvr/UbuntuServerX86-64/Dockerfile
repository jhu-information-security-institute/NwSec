# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and a caching only dns server
# Usage:
#   From this directory, run $ sudo docker build -t tdnssvr .
#   By default, runs as root

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
RUN apt-get install apt-utils dpkg-dev -y
RUN apt-get install net-tools iputils-ping iptables wget -y
RUN apt-get install vim -y

#install dependencies
RUN apt-get install systemd systemd-sysv -y

#install all the things (web server)
RUN apt-get install bind9 -y

VOLUME [ "/sys/fs/cgroup" ]
CMD ["/lib/systemd/systemd"]

# Finished!
RUN echo 'Container is ready, run it using $ sudo docker run -d --name dnssvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus="1" tdnssvr:latest'
RUN echo 'Then attach to it using $ sudo docker exec -it dnssvr bash'
RUN echo 'bind9 status is available using $ sudo systemctl status bind9'
RUN echo 'bind9 can be stopped and started as well using systemctl'
