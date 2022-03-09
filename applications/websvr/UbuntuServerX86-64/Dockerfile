# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and nginx server
# Usage:
#   From this directory, run $ docker build -t twebsvr .
#   By default, runs as root
#   https://nginx.org/en

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
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute isc-dhcp-common -y
RUN apt-get install vim acl sudo telnet ssh netcat nfs-common -y

#install dependencies for systemd and syslog
RUN apt-get install systemd systemd-sysv syslog-ng syslog-ng-core syslog-ng-mod-sql syslog-ng-mod-mongodb -y

#start systemd
CMD ["/usr/lib/systemd/systemd", "--system"]

#install all the things (web server)
RUN apt-get install nginx -y

#create the example site
RUN mkdir -p /var/www/nwsec
COPY index.html /var/www/nwsec/.

#make the example site available
COPY etc_nginx_sites-available_nwsec /etc/nginx/sites-available/nwsec
RUN ln -s /etc/nginx/sites-available/nwsec /etc/nginx/sites-enabled/nwsec

#remove the default site
RUN rm /etc/nginx/sites-enabled/default /etc/nginx/sites-available/default

# Finished!
RUN echo $'\n\
* Container is ready, run it using $ docker run -d --name websvr --hostname nginx.nwsecdocker.jhu.edu --add-host nginx.nwsecdocker.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search nwsecdocker.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus=1 twebsvr:latest \n\
* Attach to it using $ docker exec -it websvr bash \n\
* nginx status is available using # systemctl status nginx \n\
* nginx can be stopped and started as well using systemctl'
