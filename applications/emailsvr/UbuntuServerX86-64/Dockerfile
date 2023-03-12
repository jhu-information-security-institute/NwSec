# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server and postfix+dovecot+postfixadmin+lemp stack mail server environment
# Usage:
#   From this directory, run $ docker build -t temailsvr .
#   By default, runs as root
#   https://www.dovecot.org
#   http://www.postfix.org
#   http://postfixadmin.sourceforge.net
#   https://www.linuxbabe.com/mail-server/postfixadmin-ubuntu

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

#install all the things (postfix)
COPY postfix/postfix_install.sh /root/postfix_install.sh
RUN chmod +x /root/postfix_install.sh
RUN /root/postfix_install.sh
RUN apt-get install postfix -y
RUN apt-get install spamassassin spamc -y
RUN apt-get install spamass-milter -y
RUN apt-get install postfix-pcre -y

# configure postfix
COPY postfix/etc_postfix_main.cf /etc/postfix/main.cf
COPY postfix/etc_postfix_master.cf /etc/postfix/master.cf
RUN mkdir /etc/postfix/sql
COPY postfix/etc_postfix_sql_mysql_virtual_alias_domain_catchall_maps.cf /etc/postfix/sql/mysql_virtual_alias_domain_catchall_maps.cf
COPY postfix/etc_postfix_sql_mysql_virtual_alias_domain_mailbox_maps.cf /etc/postfix/sql/mysql_virtual_alias_domain_mailbox_maps.cf
COPY postfix/etc_postfix_sql_mysql_virtual_alias_domain_maps.cf /etc/postfix/sql/mysql_virtual_alias_domain_maps.cf
COPY postfix/etc_postfix_sql_mysql_virtual_alias_maps.cf /etc/postfix/sql/mysql_virtual_alias_maps.cf
COPY postfix/etc_postfix_sql_mysql_virtual_domains_maps.cf /etc/postfix/sql/mysql_virtual_domains_maps.cf
COPY postfix/etc_postfix_sql_mysql_virtual_mailbox_maps.cf /etc/postfix/sql/mysql_virtual_mailbox_maps.cf
RUN chown -R root:postfix /etc/postfix/sql
RUN chmod -R go-w /etc/postfix/sql/*.cf /etc/postfix/*.cf /etc/postfix/makedefs.out
RUN adduser vmail --system --group --uid 2000 --disabled-login --no-create-home
RUN mkdir /var/vmail
RUN chown vmail:vmail /var/vmail/ -R
RUN postconf maillog_file=/var/log/postfix.log

#install dovecot
RUN apt-get install dovecot-core dovecot-imapd dovecot-pop3d dovecot-mysql dovecot-lmtpd -y

#configure dovecot
COPY dovecot/etc_dovecot_dovecot.conf /etc/dovecot/dovecot.conf
COPY dovecot/etc_dovecot_dovecot-sql.conf.ext /etc/dovecot/dovecot-sql.conf.ext
COPY dovecot/etc_dovecot_conf.d_10-auth.conf /etc/dovecot/conf.d/10-auth.conf
COPY dovecot/etc_dovecot_conf.d_10-mail.conf /etc/dovecot/conf.d/10-mail.conf
COPY dovecot/etc_dovecot_conf.d_10-master.conf /etc/dovecot/conf.d/10-master.conf
COPY dovecot/etc_dovecot_conf.d_15-mailboxes.conf /etc/dovecot/conf.d/15-mailboxes.conf
#create dovecot and mail users
RUN adduser dovecot mail

#install all the things (web server, MariaDB database server, PHP7.4, postfixadmin)
RUN apt-get install nginx mariadb-server mariadb-client -y
RUN apt-get install php7.4 php7.4-fpm php7.4-mysql php-common php7.4-cli php7.4-common php7.4-json php7.4-opcache php7.4-readline php7.4-mbstring php7.4-xml php7.4-gd php7.4-curl -y

#remove the default nginx site
RUN rm /etc/nginx/sites-enabled/default /etc/nginx/sites-available/default

RUN apt-get install dbconfig-no-thanks -y
RUN apt-get install postfixadmin -y
RUN apt-get remove dbconfig-no-thanks -y

COPY postfixadmin/etc_nginx_conf.d_postfixadmin.conf /etc/nginx/conf.d/postfixadmin.conf
RUN mkdir -p /usr/share/postfixadmin/templates_c
RUN chown -R www-data /usr/share/postfixadmin/templates_c
COPY postfixadmin/usr_share_postfixadmin_config.local.php /usr/share/postfixadmin/config.local.php

RUN ln -s /usr/share/postfixadmin/public /usr/share/nginx/html/postfixadmin

RUN echo "mail" >> /etc/hostname

# Finished!
RUN echo $'\n\
* Container is ready, run it using $ docker run -d --name emailsvr --hostname mail --add-host mail.netsec-docker.isi.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search netsec-docker.isi.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro --network host --cpus=1 temailsvr:latest \n\
* Attach to it using $ docker exec -it emailsvr bash \n\
* Build the postfixadmin mysql database # dpkg-reconfigure postfixadmin \n\
    * Specify these values when prompted: \n\
        * Reinstall database for postfixadmin? [yes/no] yes \n\
        * Connection method for MySQL database of postfixadmin: 1 \n\
        * Authentication plugin for MySQL database: 1 \n\
        * MySQL database name for postfixadmin: postfixadmin \n\
        * MySQL username for postfixadmin: postfixadmin@localhost \n\
        * MySQL application password for postfixadmin: nwsec123 \n\
        * Password confirmation: nwsec123 \n\
        * Name of the database administrative user: root \n\
* Edit /etc/postfixadmin/dbconfig.inc.php and change value of line with $dbtype from mysql to mysqli \n\
* Edit /etc/dbconfig-common/postfixadmin.conf and change value of line with dbc_dbtype from mysql to mysqli \n\
* Log in to http://mail.netsec-docker.isi.jhu.edu/postfixadmin/setup.php \n\
    * Specify a setup password and copy the generated hash into the value for the setup_password variable in /etc/postfixadmin/config.inc.php \n\
    * Then, create a superadmin account Admin postfixadmin@netsec-docker.isi.jhu.edu and specify its password \n\
* Use that superadmin account to log in to http://mail.netsec-docker.isi.jhu.edu/postfixadmin/login.php \n\
    * Create a new netsec-docker.isi.jhu.edu domain \n\
    * Create new email accounts in the netsec-docker.isi.jhu.edu domain \n\
    * Note, you will need to create an email account for superadmin user \n\
* Use Mozilla Thunderbird for a mail client from the VM or another container to access email'
