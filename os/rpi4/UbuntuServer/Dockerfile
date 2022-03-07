# Description:
#   This runtime environment example Dockerfile creates a container with a minimal Ubuntu server for cross-compiling arm
# Usage:
#   From this directory, run $ docker build -t tccarm .
#   By default, runs as root

FROM ubuntu:20.04

#avoid question/dialog during apt-get installs
ARG DEBIAN_FRONTEND noninteractive

# Setup container's ENV for systemd
ENV container docker

#update
RUN apt-get update

RUN apt-get install xauth -y

#install dependencies for systemd and syslog
RUN apt-get install systemd systemd-sysv syslog-ng syslog-ng-core syslog-ng-mod-sql syslog-ng-mod-mongodb -y

#start systemd
CMD ["/usr/lib/systemd/systemd", "--system"]

#install utilities
RUN apt-get install apt-utils dpkg-dev debconf debconf-utils pkg-config -y
RUN apt-get install libgtk-3-0 x11-apps -y
RUN apt-get install acl sudo -y
RUN apt-get install wget vim -y
RUN apt-get install usbutils -y
RUN apt-get install git bison gettext texinfo cpio python zlib1g-dev libfdt-dev build-essential binutils gcc-multilib g++-multilib gdb-multiarch nasm cowsay gcc-arm-none-eabi binutils-arm-none-eabi libnewlib-arm-none-eabi -y

#copy setup_user.sh over to the container
COPY setup_user.sh /root

#For convenience, replicate /etc/sudoers.d on VM to this container \n\ 
#First remove the default one from the container
RUN rm -R /etc/sudoers.d
#Then replicate from host
VOLUME "/etc/sudoers.d"

# Finished!
RUN echo $'\n\
* For X11 forwarding to work via /tmp/.X11-unix, mount same folder and /home/$USER/.Xauthority as rw volumes and also use --network host \n\
* For convenience, bind mount (ro) /etc/group, /etc/passwd, /etc/shadow on VM to this container \n\ 
* Container is ready, run it using $ docker run -d --name ccarm --privileged -e DISPLAY=$DISPLAY -v /sys/fs/cgroup:/sys/fs/cgroup:ro -v /tmp/.X11-unix:/tmp/.X11-unix:rw -v /etc/group:/etc/group:ro -v /etc/passwd:/etc/passwd:ro -v /etc/shadow:/etc/shadow:ro -v /home/$USER/.Xauthority:/home/$USER/.Xauthority:rw -v /dev/bus/usb:/dev/bus/usb --network host --privileged tccarm:latest \n\
* Attach to it using $ docker exec -it ccarm bash \n\
* Finish setup of your <USER> by running # /root/setup_user.sh -u <USER> -H <HOME> \n\
* Make sure that the container is authenticated with the host (cookie for the container should be present in .Xauthority file) \n\
* Build!'


