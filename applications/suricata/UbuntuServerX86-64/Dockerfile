# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and suricata
# Usage:
#   From this directory, run $ docker build -t tsuricata .
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
ENV HOSTNAME suricata.nwsecdocker.jhu.edu

#update
RUN apt-get update

#install utilities
RUN apt-get install apt-utils dpkg-dev debconf debconf-utils -y
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute -y
RUN apt-get install vim acl sudo telnet ssh netcat -y

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
RUN mv /etc/default/suricata /etc/default/suricata.orig
COPY suricata.yaml /etc/suricata/suricata.yaml
COPY etc_default_suricata /etc/default/suricata
RUN suricata-update
COPY var_lib_suricata_rules_test-ddos.rules /var/lib/suricata/rules/test-ddos.rules

VOLUME [ "/sys/fs/cgroup" ]
CMD ["/lib/systemd/systemd"]

RUN echo $'\n\
* Container is ready, run it using $ docker run -d --name suricata --hostname suricata.nwsecdocker.jhu.edu --add-host suricata.nwsecdocker.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search nwsecdocker.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus="2" tsuricata:latest \n\
* Attach to it using $ docker exec -it suricata bash \n\
* suricata status is available using # systemctl status suricata \n\
* suricata can be stopped and started as well using systemctl'
