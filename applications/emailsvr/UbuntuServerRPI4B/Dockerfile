# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and postfix+dovecot+postfixadmin+lemp stack mail server environment
# Usage:
#   From this directory, run $ sudo docker build -t temailsvr .
# By default, runs as root
FROM ubuntu:18.04

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

#install all the things (email)
RUN apt-get install postfix -y
RUN apt-get install dovecot-core dovecot-imapd dovecot-pop3d -y
RUN adduser dovecot mail

COPY etc_postfix_main.cf /etc/postfix/main.cf
COPY etc_postfix_master.cf /etc/postfix/master.cf
COPY etc_dovecot_dovecot.conf /etc/dovecot/dovecot.conf
COPY etc_dovecot_conf.d_10-auth.conf /etc/dovecot/conf.d/10-auth.conf
COPY etc_dovecot_conf.d_10-mail.conf /etc/dovecot/conf.d/10-mail.conf
COPY etc_dovecot_conf.d_10-master.conf /etc/dovecot/conf.d/10-master.conf
COPY etc_dovecot_conf.d_15-mailboxes.conf /etc/dovecot/conf.d/15-mailboxes.conf

# install X11 dependencies
RUN apt-get install libxext6 libxtst6 libxi6 dbus-x11 xauth -y

# install gnome-terminal
RUN apt-get install --no-install-recommends gnome-terminal -y

VOLUME [ "/sys/fs/cgroup" ]

# Finished!
RUN echo 'Container is ready, run it using $ sudo docker run -d --name emailsvr --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host temailsvr:latest'
RUN echo 'Then attach to it using $ sudo docker exec -it emailsvr bash'

CMD ["/lib/systemd/systemd"]
