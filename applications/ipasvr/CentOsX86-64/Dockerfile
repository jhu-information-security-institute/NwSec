# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Centos server and ipa server
# Usage:
#   From this directory, run $ docker build -t tipasvr .
#   By default, runs as root

FROM quay.io/centos/centos:stream9

#https://grigorkh.medium.com/fix-tzdata-hangs-docker-image-build-cdb52cc3360d
ENV TZ=US/Eastern
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

#avoid question/dialog during apt-get installs
ARG DEBIAN_FRONTEND noninteractive

# Setup container's ENV for systemd
ENV container docker

#update
RUN yum update

#install utilities
RUN yum install net-tools iputils wget nmap bind-utils -y
RUN yum install ncurses vim-enhanced acl sudo openssh-clients -y

#install dependencies for systemd and syslog
RUN yum install systemd -y

#start systemd
CMD ["/usr/lib/systemd/systemd", "--system"]

#install all the things (ipa server)
RUN yum install krb5-workstation krb5-libs -y
RUN yum install freeipa-server -y

# Finished!
RUN echo $'\n\    
* Container is ready, run it using $ docker run -d --name ipasvr --hostname ipa.netsec.isi.jhu.edu --privileged --network host --cpus=2 tipasvr:latest \n\
* Attach to it using $ docker exec -it ipasvr bash'
