# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and postfix+dovecot+postfixadmin+lemp stack mail server environment
# Usage:
#   From this directory, run $ sudo docker build -t temailsvr .
#   By default, runs as root

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

#install X11 dependencies
RUN apt-get install libxext6 libxtst6 libxi6 dbus-x11 xauth -y

#install gnome-terminal
RUN apt-get install --no-install-recommends gnome-terminal -y

#install all the things (email)
RUN apt-get install debconf -y
COPY postfix_install.sh /root/postfix_install.sh
RUN /root/postfix_install.sh
RUN apt-get install postfix -y
RUN apt-get install dovecot-core dovecot-imapd dovecot-pop3d -y

#create dovecot and mail users
RUN adduser dovecot mail

# configure postfix and dovecot
COPY etc_postfix_main.cf /etc/postfix/main.cf
COPY etc_postfix_master.cf /etc/postfix/master.cf
COPY etc_dovecot_dovecot.conf /etc/dovecot/dovecot.conf
COPY etc_dovecot_conf.d_10-auth.conf /etc/dovecot/conf.d/10-auth.conf
COPY etc_dovecot_conf.d_10-mail.conf /etc/dovecot/conf.d/10-mail.conf
COPY etc_dovecot_conf.d_10-master.conf /etc/dovecot/conf.d/10-master.conf
COPY etc_dovecot_conf.d_15-mailboxes.conf /etc/dovecot/conf.d/15-mailboxes.conf

#install all the things (web server, MariaDB database server, PHP7.4, phpMyAdmin)
RUN apt-get install nginx mariadb-server mariadb-client -y
RUN apt-get install php7.4 php7.4-fpm php7.4-mysql php-common php7.4-cli php7.4-common php7.4-json php7.4-opcache php7.4-readline php7.4-mbstring php7.4-xml php7.4-gd php7.4-curl -y
COPY phpmyadmin_install.sh /root/phpmyadmin_install.sh
RUN /root/phpmyadmin_install.sh
RUN apt-get install phpmyadmin -y

#test
#COPY default /etc/nginx/sites-available/default
#COPY info.php /usr/share/nginx/html/info.php

#remove the default site
RUN rm /etc/nginx/sites-enabled/default /etc/nginx/sites-available/default

COPY phpmyadmin.conf /etc/nginx/conf.d/phpmyadmin.conf
COPY dovecot.conf /etc/dovecot/dovecot.conf

VOLUME [ "/sys/fs/cgroup" ]
CMD ["/lib/systemd/systemd"]

# Finished!
RUN echo 'Container is ready, run it using $ docker run -d --name emailsvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host temailsvr:latest'
RUN echo 'Then attach to it using $ docker exec -it emailsvr bash'
RUN echo 'Then run # hostnamectl set-hostname mymail.netsec.isi.jhu.edu'
RUN echo 'Open a browser and go to http://localhost, phpMyAdmin login is root, password'
