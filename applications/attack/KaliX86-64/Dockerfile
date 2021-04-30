# Description:
#   This runtime environment example Dockerfile creates a container for attacking and demonstrations.  It
#   can run X11 applications from within the container.
# Usage:
#   From this directory, run $ docker build -t tattack .
#   By default, runs as root
# https://docs.docker.com/network/macvlan
# http://fabiorehm.com/blog/2014/09/11/running-gui-apps-with-docker

FROM kalilinux/kali:latest

#avoid question/dialog during apt-get installs
ARG DEBIAN_FRONTEND noninteractive

#update
RUN apt-get update

RUN apt-get install xauth -y

#install dependencies for systemd and syslog
RUN apt-get install systemd systemd-sysv syslog-ng syslog-ng-core syslog-ng-mod-sql syslog-ng-mod-mongodb -y

#start systemd
CMD ["/usr/lib/systemd/systemd", "--system"]

#install utilities
RUN apt-get install apt-utils dpkg-dev debconf debconf-utils -y
RUN apt-get install net-tools iputils-ping iptables iproute2 wget nmap bind9-dnsutils dnsutils inetutils-traceroute isc-dhcp-common -y
RUN apt-get install vim acl sudo telnet ssh netcat nfs-common -y
RUN apt-get install hping3 -y
RUN apt-get install hydra hydra-gtk ismtp set -y

RUN apt-get install thunderbird firefox-esr sendmail -y

#Must create a directory for any host VM users you intend to also use in the container
RUN mkdir -p $HOME
COPY setup_user.sh /root/setup_user.sh

#For convenience, replicate /etc/sudoers.d on VM to this container \n\ 
#First remove the default one from the container
RUN rm -R /etc/sudoers.d
#Then replicate from host
VOLUME "/etc/sudoers.d"

# Finished!
RUN echo $'\n\
* For X11 forwarding to work via /tmp/.X11-unix, mount same folder and /home/$USER/.Xauthority as rw volumes and also use --network host \n\
* For convenience, bind mount (ro) /etc/group, /etc/passwd, /etc/shadow on VM to this container \n\ 
* Container is ready, run it using $ docker run -d --name attack --hostname attack --add-host attack:127.0.1.1 --dns 192.168.25.10 --dns-search nwsecdocker.jhu.edu --privileged -e DISPLAY=$DISPLAY -v /sys/fs/cgroup:/sys/fs/cgroup:ro -v /tmp/.X11-unix:/tmp/.X11-unix:rw -v /etc/group:/etc/group:ro -v /etc/passwd:/etc/passwd:ro -v /etc/shadow:/etc/shadow:ro -v /home/$USER/.Xauthority:/home/$USER/.Xauthority:rw --network host tattack:latest \n\
* Attach to it using $ docker exec -it attack bash \n\
* Finish setup of your <USER> by running # /root/setup_user.sh -u <USER> -H <HOME> \n\
* After the container is exited, you can restart and relogin to it using $ docker start -i attack \n\
* Attack!'


