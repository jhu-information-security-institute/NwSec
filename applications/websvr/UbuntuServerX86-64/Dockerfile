# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and nginx server
# Usage:
#   From this directory, run $ sudo docker build -t twebsvr .
#   By default, runs as root
#   https://nginx.org/en/

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

#install dependencies
RUN apt-get install systemd systemd-sysv -y

#install all the things (web server)
RUN apt-get install nginx -y

#create the example site
RUN mkdir -p /var/www/nwsec
COPY index.html /var/www/nwsec/.

#make the example site available
COPY nwsec /etc/nginx/sites-available/.
RUN ln -s /etc/nginx/sites-available/nwsec /etc/nginx/sites-enabled/nwsec

#remove the default site
RUN rm /etc/nginx/sites-enabled/default /etc/nginx/sites-available/default

VOLUME [ "/sys/fs/cgroup" ]
CMD ["/lib/systemd/systemd"]

# Finished!
RUN echo 'Container is ready, run it using $ sudo docker run -d --name websvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus="1" twebsvr:latest'
RUN echo 'Then attach to it using $ sudo docker exec -it websvr bash'
RUN echo 'nginx status is available using $ sudo systemctl status nginx'
RUN echo 'nginx can be stopped and started as well using systemctl'
