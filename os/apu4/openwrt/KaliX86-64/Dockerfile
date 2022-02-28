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

# Get the dependencies for openwrt (see https://openwrt.org/docs/guide-developer/toolchain/install-buildsystem)
RUN apt-get install build-essential ccache ecj fastjar file g++ gawk \
gettext git java-propose-classpath libelf-dev libncurses5-dev \
libncursesw5-dev libssl-dev python python2.7-dev python3 unzip wget \
python3-distutils python3-setuptools python3-dev rsync subversion \
swig time xsltproc zlib1g-dev -y


# Get useful utilities
RUN apt-get install apt-utils net-tools iputils-ping openssh-server terminator vim binutils -y

# Install X dependencies
RUN apt-get install libxext6 libxtst6 libxi6 xauth -y

COPY scripts/build.sh $CONTMNTSCRIPTS/.
COPY scripts/create_user.sh $CONTMNTSCRIPTS/.

# Finished!
RUN echo 'Container is ready, run it using $ docker run -d --name openwrtapu4buildenv -v scripts:/mnt/hgfs/scripts -v /mnt/hgfs/shared:/mnt/hgfs/shared -v /mnt/hgfs/sandbox:/mnt/hgfs/sandbox -t t_openwrtapu4buildenv:latest'
RUN echo 'Attach to the container by running $ docker exec -it openwrtapu4buildenv bash'
RUN echo 'Create your user by running $ /mnt/hgfs/scripts/create_user.sh'
RUN echo 'Switch to your user by running $ su <USERNAME>'
RUN echo 'Run the build script via $ /mnt/hgfs/scripts/build.sh'
RUN echo 'Follow the notes from the markdown on the configuration settings for menuconfig'
RUN echo 'Binary will be located in /mnt/hgfs/scripts/openwrt/bin/targets/x86/64'