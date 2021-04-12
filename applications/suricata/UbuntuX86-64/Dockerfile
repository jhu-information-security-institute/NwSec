# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and suricata
# Usage:
#   From this directory, run $ sudo docker build -t tsuricata .
# By default, runs as root
# https://suricata-ids.org/
FROM ubuntu:20.04

#https://grigorkh.medium.com/fix-tzdata-hangs-docker-image-build-cdb52cc3360d
ENV TZ=US/Eastern
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

RUN echo 'Install dependencies'
RUN apt-get update
RUN apt-get install -y net-tools wget libpcre3 libpcre3-dbg libpcre3-dev iputils-ping vim iptables
RUN apt-get install -y build-essential autoconf automake libtool libpcap-dev libnet1-dev libyaml-0-2 libyaml-dev 
RUN apt-get install -y zlib1g zlib1g-dev libcap-ng-dev libcap-ng0 make libmagic-dev libjansson-dev libjansson4 pkg-config

#install suricata
RUN apt-get install -y software-properties-common
RUN add-apt-repository ppa:oisf/suricata-stable
RUN apt-get update
RUN apt-get install -y suricata
RUN wget http://rules.emergingthreats.net/open/suricata/emerging.rules.tar.gz
RUN tar -xvzf emerging.rules.tar.gz
RUN mkdir /var/lib/suricata/
RUN mv rules /var/lib/suricata
RUN rm emerging.rules.tar.gz
RUN mv /etc/suricata/suricata.yaml /etc/suricata/suricata.yaml.orig
COPY suricata.yaml /etc/suricata/suricata.yaml

RUN echo 'Container is ready, run it interactively using $ sudo docker run --name suricata --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host -ti tsuricata:latest bash'
RUN echo 'Then, inside the container, start suricata using # suricata -c /etc/suricata/suricata.yaml -i ens39'
