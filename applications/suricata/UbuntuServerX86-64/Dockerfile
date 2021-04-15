# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and suricata
# Usage:
#   From this directory, run $ sudo docker build -t tsuricata .
#   Before running, should have already setup bridge br0 by running $ sudo ./create-bridge.sh
#   Also before running, remove redundant routes by running $ sudo ./setup-routes.sh
#   By default, runs as root
#   https://suricata-ids.org

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

RUN echo 'Install dependencies'
RUN apt-get install systemd systemd-sysv -y
RUN apt-get install -y libpcre3 libpcre3-dbg libpcre3-dev 
RUN apt-get install -y build-essential autoconf automake libtool libpcap-dev libnet1-dev libyaml-0-2 libyaml-dev 
RUN apt-get install -y zlib1g zlib1g-dev libcap-ng-dev libcap-ng0 make libmagic-dev libjansson-dev libjansson4 pkg-config

#install all the things (suricata)
RUN apt-get install -y software-properties-common
RUN add-apt-repository ppa:oisf/suricata-stable
RUN apt-get update
RUN apt-get install -y suricata jq

#install some rules and configure suricata
RUN wget http://rules.emergingthreats.net/open/suricata/emerging.rules.tar.gz
RUN tar -xvzf emerging.rules.tar.gz
RUN mkdir /var/lib/suricata/
RUN mv rules /var/lib/suricata
RUN rm emerging.rules.tar.gz
RUN mv /etc/suricata/suricata.yaml /etc/suricata/suricata.yaml.orig
COPY suricata.yaml /etc/suricata/suricata.yaml

VOLUME [ "/sys/fs/cgroup" ]
CMD ["/lib/systemd/systemd"]

RUN echo 'Container is ready, run it using $ sudo docker run -d --name suricata --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus="2" tsuricata:latest'
RUN echo 'Then attach to it using $ sudo docker exec -it suricata bash'
RUN echo 'suricata status is available using $ sudo systemctl status suricata'
RUN echo 'suricata can be stopped and started as well using systemctl'
