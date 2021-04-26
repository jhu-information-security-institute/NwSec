# Description:
#   This runtime environment example Dockerfile creates a container with a ELK server
# Usage:
#   From this directory, run $ docker build -t telksvr .
#   By default, runs as root
# https://www.elastic.co/guide/en/elasticsearch/reference/7.12/install-elasticsearch.html
# https://www.elastic.co/guide/en/logstash/7.12/installing-logstash.html
# https://www.elastic.co/guide/en/kibana/7.12/install.html

FROM ubuntu:20.04

#https://grigorkh.medium.com/fix-tzdata-hangs-docker-image-build-cdb52cc3360d
ENV TZ=US/Eastern
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

#avoid question/dialog during apt-get installs
ARG DEBIAN_FRONTEND noninteractive

# Setup container's ENV for systemd
ENV container docker
ENV HOSTNAME elk.nwsecdocker.jhu.edu

#update
RUN apt-get update

#install utilities
RUN apt-get install apt-utils dpkg-dev debconf debconf-utils -y
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute openssh-server isc-dhcp-common -y
RUN apt-get install vim acl sudo telnet ssh netcat nfs-common -y

#install dependencies
RUN apt-get install systemd systemd-sysv -y

#install all the things (Elasticsearch, Logstash, Kibana)
RUN apt-get install apt-transport-https
RUN wget -qO - https://artifacts.elastic.co/GPG-KEY-elasticsearch | sudo apt-key add -
RUN echo "deb https://artifacts.elastic.co/packages/7.x/apt stable main" | sudo tee /etc/apt/sources.list.d/elastic-7.x.list
RUN apt-get update
RUN apt-get install elasticsearch logstash kibana

VOLUME [ "/sys/fs/cgroup" ]
CMD ["/lib/systemd/systemd"]

# Finished!
RUN echo $'\n\
* Container is ready, run it using $ docker run -d --name elksvr --hostname elk.nwsecdocker.jhu.edu --add-host elk.nwsecdocker.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search nwsecdocker.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus="1" telksvr:latest \n\
* Attach to it using $ docker exec -it elksvr bash \n\
* Enable the services by running: # systemctl enable elasticsearch logstash kibana \n\
* Status is available using: \n\
    * # systemctl status elasticsearch \n\
    * # systemctl status logstash \n\
    * # systemctl status kibana \n\
* One can similarly stop/start these services using systemctl'
