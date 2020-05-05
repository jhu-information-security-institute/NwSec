# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and nfs server
# Usage:
#   From this directory, run $ sudo docker build -t tnfssvr .
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

#install all the things (nfs server)
RUN apt-get install nfs-kernel-server -y

#create the export directory
RUN mkdir -p /export/nfsshare

VOLUME [ "/sys/fs/cgroup" ]
VOLUME [ "/export/nfsshare" ]
VOLUME [ "/nfsshare" ]

# Finished!
RUN echo 'Container is ready, run it using $ sudo docker run -d --name nfssvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro -v /export/nfsshare:/export/nfsshare:rw -v /nvsshare:/nfsshare:rw --network host tnfssvr:latest'
RUN echo 'Then attach to it using $ sudo docker exec -it nfssvr bash'

CMD ["/lib/systemd/systemd"]
