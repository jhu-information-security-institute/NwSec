# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and nginx server
# Usage:
#   From this directory, run $ sudo docker build -t twebsvr .
# By default, runs as root
FROM ubuntu:20.04

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
RUN apt-get install iptables -y

#install all the things (web server)
RUN apt-get install nginx -y

#create the example site
RUN mkdir -p /var/www/nwsec
COPY index.html /var/www/nwsec/.
COPY nwsec /etc/nginx/sites-available/.
RUN ln -s /etc/nginx/sites-available/nwsec /etc/nginx/sites-enabled/nwsec

VOLUME [ "/sys/fs/cgroup" ]

# Finished!
RUN echo 'Container is ready, run it using $ sudo docker run -d --name websvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network br0 --ip 192.168.25.10 --publish 8081:81 twebsvr:latest'
RUN echo 'Then attach to it using $ sudo docker exec -it websvr bash'
RUN echo 'nginx status is available using $ sudo systemctl status nginx'
RUN echo 'nginx can be stopped and started as well using systemctl'
RUN echo 'enable ip forwarding from Docker containers to outside world by running $ sudo ./enable-forwarding'

CMD ["/lib/systemd/systemd"]
