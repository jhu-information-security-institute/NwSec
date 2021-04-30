# Description:
#   This runtime environment example Dockerfile creates a container with a telnet server and a ssh server
# Usage:
#   From this directory, run $ docker build -t ttermsvr .
#   By default, runs as root
# https://www.gnu.org/software/inetutils/
# https://ubuntu.com/server/docs/service-openssh

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
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute openssh-server isc-dhcp-common -y
RUN apt-get install vim acl sudo telnet ssh netcat nfs-common -y

#install dependencies for systemd and syslog
RUN apt-get install systemd systemd-sysv syslog-ng syslog-ng-core syslog-ng-mod-sql syslog-ng-mod-mongodb -y

#start systemd
CMD ["/usr/lib/systemd/systemd", "--system"]

#install all the things (inetutils-telnetd, openssh-server)
RUN apt-get install inetutils-telnetd openssh-server -y

COPY etc_ssh_sshd_config /etc/ssh/sshd_config
COPY etc_pam.d_login /etc/pam.d/login
COPY etc_inetd.conf /etc/inetd.conf

#create a few easy to guess users and passwords
RUN useradd -m student
RUN echo 'student:nwsec' | chpasswd
RUN useradd -m john
RUN echo 'john:password' | chpasswd
RUN useradd -m jane
RUN echo 'jane:password' | chpasswd

#create a goldenkeyfile
RUN echo 'reubwashere' > /root/goldenkey

# Finished!
RUN echo $'\n\
* Container is ready, run it using $ docker run -d --name termsvr --hostname term.nwsecdocker.jhu.edu --add-host term.nwsecdocker.jhu.edu:127.0.1.1 --dns 192.168.25.10 --dns-search nwsecdocker.jhu.edu --privileged -v /sys/fs/cgroup:/sys/fs/cgroup:ro -v /nfsshare:/nfsshare:rw --network host --cpus=1 ttermsvr:latest \n\
* Attach to it using $ docker exec -it termsvr bash \n\
* inetutils-telnetd status is available using # systemctl status inetutils-inetd \n\
* openssh-server status is available using # systemctl status sshd \n\
* inetutils-inetd can be stopped and started as well using systemctl \n\
* openssh-server can be stopped and started as well using systemctl \n\
* telnet into the server by running $ telnet term.nwsecdocker.jhu.edu \n\
* ssh into the server by running $ ssh userterm.nwsecdocker.jhu.edu -p 10022 \n\
* Build the goroot executable and set its capabilities using # setcap CAP_SETUID+ep goroot \n\
* Then upload goroot to the nfsshare'
