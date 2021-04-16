# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and suricata
# Usage:
#   From this directory, run $ sudo docker build -t tsuricata .
#   Before running, should have already setup bridge br0, e.g., by running $ sudo ../../../tools/bridge/create-bridge.sh -n br0 -d ens38 -g 192.168.25.1/24
#   Also before running, remove redundant routes by running $ sudo ../../../tools/bridge/setup-routes.sh -d ens38 -m 192.168.25.0/24
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

#configure suricata and install some rules
RUN mv /etc/suricata/suricata.yaml /etc/suricata/suricata.yaml.orig
COPY suricata.yaml /etc/suricata/suricata.yaml
RUN suricata-update
COPY var_lib_suricata_rules_test-ddos.rules /var/lib/suricata/rules/test-ddos.rules

VOLUME [ "/sys/fs/cgroup" ]
CMD ["/lib/systemd/systemd"]

RUN echo 'Container is ready, run it using $ sudo docker run -d --name suricata --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus="2" tsuricata:latest'
RUN echo 'Then attach to it using $ sudo docker exec -it suricata bash'
RUN echo 'suricata status is available using $ sudo systemctl status suricata'
RUN echo 'suricata can be stopped and started as well using systemctl'
