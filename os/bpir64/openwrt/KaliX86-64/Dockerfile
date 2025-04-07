# Description:
#   This Dockerfile creates a container with a build environment for OpenWrt on bpir64.
#   https://openwrt.org/docs/guide-developer/start
# Usage:
#   From this directory, run $ docker build --build-arg CONTMNTSANDBOX=/mnt/docker/sandbox -t t_openwrtbpir64buildenv .

# By default, runs as root

# Pull kali-last-release:latest
FROM kalilinux/kali-last-release:latest

# arguments:
ARG CONTMNTSANDBOX #e.g., /mnt/docker/sandbox

# Mount the shared directories
RUN mkdir -p $CONTMNTSANDBOX
RUN chmod -R uga+w $CONTMNTSANDBOX

RUN apt-get update

# Get useful utilities
RUN apt-get install -y apt-utils net-tools iputils-ping openssh-server terminator vim binutils

# Get the dependencies for openwrt (see https://openwrt.org/docs/guide-developer/toolchain/install-buildsystem)
RUN apt-get install -y build-essential clang flex bison g++ gawk \
gcc-multilib g++-multilib gettext git libncurses5-dev libssl-dev \
python3-setuptools rsync swig unzip zlib1g-dev file wget

# Install X dependencies
RUN apt-get install -y libxext6 libxtst6 libxi6 xauth

# Finished!
RUN echo 'Container is ready, run it using $ docker run -d --name openwrtbpir64buildenv -v /tmp/.X11-unix:/tmp/.X11-unix:ro -v <PATHTOSANDBOX>:/mnt/docker/sandbox -t t_openwrtbpir64buildenv:latest'
RUN echo 'Attach to the container by running $ docker exec -it openwrtbpir64buildenv bash'
RUN echo 'Create your user by running $ /mnt/docker/sandbox/<PATHTOFOLDER>/create_user.sh -u <USERNAME> -p <PASSWORD>'
RUN echo 'Switch to your user by running $ su <USERNAME>'
RUN echo 'Run the build script via $ /mnt/docker/scripts/build.sh'
RUN echo 'Binary will be located in /mnt/docker/sandbox/openwrt/bin/targets/x86/64'
