# Description:
#   This Dockerfile creates a container with a build environment for OpenWrt on apu4.
#   https://openwrt.org/docs/guide-developer/start
# Usage:
#   From this directory, run $ sudo docker build --build-arg CONTMNTSCRIPTS=/mnt/docker/scripts --build-arg CONTMNTSANDBOX=/mnt/docker/sandbox -t t_openwrtapu4buildenv .

# By default, runs as root

# Pull kali-last-release:latest
FROM kalilinux/kali-last-release:latest

# arguments:
ARG CONTMNTSCRIPTS #e.g., /mnt/docker/scripts
ARG CONTMNTSANDBOX #e.g., /mnt/docker/sandbox

# Mount the shared directories
RUN mkdir -p $CONTMNTSCRIPTS
RUN mkdir -p $CONTMNTSANDBOX
RUN chmod -R uga+w $CONTMNTSCRIPTS
RUN chmod -R uga+w $CONTMNTSANDBOX

RUN apt-get update

# Get useful utilities
RUN apt-get install apt-utils net-tools iputils-ping openssh-server terminator vim binutils -y

# Get the dependencies for openwrt (see https://openwrt.org/docs/guide-developer/toolchain/install-buildsystem)
RUN apt-get install build-essential ccache ecj fastjar file g++ gawk \
gettext git java-propose-classpath libelf-dev libncurses5-dev \
libncursesw5-dev libssl-dev python2.7-dev python3 unzip wget \
python3-distutils python3-setuptools python3-dev rsync subversion \
swig time xsltproc zlib1g-dev -y

# Install X dependencies
RUN apt-get install libxext6 libxtst6 libxi6 xauth -y

COPY scripts/build.sh $CONTMNTSCRIPTS/.
COPY scripts/create-user.sh $CONTMNTSCRIPTS/.
COPY diffconfig $CONTMNTSANDBOX/.

# Finished!
RUN echo 'Container is ready, run it using $ docker run -d --name openwrtapu4buildenv -v scripts:/mnt/docker/scripts -t t_openwrtapu4buildenv:latest'
RUN echo 'Attach to the container by running $ docker exec -it openwrtapu4buildenv bash'
RUN echo 'Create your user by running $ /mnt/docker/scripts/create_user.sh -u <USERNAME> -p <PASSWORD>'
RUN echo 'Switch to your user by running $ su <USERNAME>'
RUN echo 'Run the build script via $ /mnt/docker/scripts/build.sh'
RUN echo 'Binary will be located in /mnt/docker/sandbox/openwrt/bin/targets/x86/64'