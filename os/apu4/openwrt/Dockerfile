# Description:
#   This Dockerfile creates a container with a build environment for OpenWrt on apu4.
#   https://openwrt.org/docs/guide-developer/start
# Usage:
#   From this directory, run $ sudo docker build --build-arg CONTMNTSCRIPTS=/mnt/hgfs/scripts --build-arg CONTMNTSHARED=/mnt/hgfs/shared --build-arg CONTMNTSANDBOX=/mnt/hgfs/sandbox -t t_openwrtapu4buildenv .

# By default, runs as root

# Pull kali-last-release:latest
FROM kalilinux/kali-last-release:latest

# arguments:
ARG CONTMNTSCRIPTS
#e.g., /mnt/hgfs/scripts
ARG CONTMNTSHARED
#e.g., /mnt/hgfs/shared
ARG CONTMNTSANDBOX
#e.g., /mnt/hgfs/sandbox

# Mount the shared directories
RUN mkdir -p $CONTMNTSCRIPTS
RUN mkdir -p $CONTMNTSHARED
RUN mkdir -p $CONTMNTSANDBOX

RUN apt-get update

# Get the dependencies for openwrt
RUN apt-get install subversion g++ zlib1g-dev build-essential git python python3 \
libncurses5-dev gawk gettext unzip file libssl-dev wget \
libelf-dev ecj fastjar java-propose-classpath \
libncursesw5-dev -y

# Get useful utilities
RUN apt-get install apt-utils net-tools iputils-ping openssh-server terminator vim binutils -y

# Install X dependencies
RUN apt-get install libxext6 libxtst6 libxi6 xauth -y

# Finished!
RUN echo 'Container is ready, run it using $ docker run -d --name openwrtapu4buildenv -v /mnt/hgfs/scripts:/mnt/hgfs/scripts -v /mnt/hgfs/shared:/mnt/hgfs/shared -v /mnt/hgfs/sandbox:/mnt/hgfs/sandbox -t t_openwrtapu4buildenv:latest'
#cd /mnt/hgfs/scripts
#run create-user.sh
#su username
#run display-runtime.sh
