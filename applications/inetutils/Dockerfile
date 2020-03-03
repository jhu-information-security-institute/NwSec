# Description:
#   This Dockerfile creates a container with a minimal Ubuntu server
# Usage:
#   From this directory, run $ sudo docker build -t ubuntuserverenv .
# By default, runs as root
FROM ubuntu:18.04
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get update
RUN apt-get upgrade -y
RUN apt-get install apt-utils dpkg-dev net-tools iputils-ping openssh-server -y
RUN apt-get install build-essential vim autotools-dev autoconf lp2man -y
COPY sources.list /etc/apt/sources.list
RUN apt-get update
# Finished!
RUN echo 'Container is ready, run it using $ sudo docker run --name ubuntuserver -it ubuntuserverenv:latest'
