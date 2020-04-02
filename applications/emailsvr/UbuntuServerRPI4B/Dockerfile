# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and telnetd server
# Usage:
#   From this directory, run $ sudo docker build -t temailsvr .
# By default, runs as root
FROM ubuntu:18.04

ENV DEBIAN_FRONTEND noninteractive
# Setup ENV for systemd
ENV container docker

#update and upgrade
RUN apt-get update
RUN apt-get upgrade -y

#install utilities and dependencies
RUN apt-get install apt-utils dpkg-dev net-tools iputils-ping -y
RUN apt-get install vim -y
RUN apt-get install systemd systemd-sysv -y

#install all the things (email)
RUN apt-get install citadel-server citadel-webcit citadel-mta citadel-client -y

RUN mkdir -p /etc/citadel/netconfigs
RUN chown citadel:citadel /etc/citadel/netconfigs

VOLUME [ "/sys/fs/cgroup" ]

# Finished!
RUN echo 'Container is ready, run it using $ sudo docker run -d --name emailsvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host temailsvr:latest'
RUN echo 'Then attach to it using $ sudo docker exec -it emailsvr bash'

CMD ["/lib/systemd/systemd"]
