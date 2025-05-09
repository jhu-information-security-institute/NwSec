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
RUN apt-get install -y apt-utils net-tools iputils-ping openssh-server terminator vim binutils

# Get the dependencies for openwrt (see https://openwrt.org/docs/guide-developer/toolchain/install-buildsystem)
RUN apt-get install -y build-essential clang flex bison g++ gawk \
gcc-multilib g++-multilib gettext git libncurses5-dev libssl-dev \
python3-setuptools rsync swig unzip zlib1g-dev file wget

# Install X dependencies
RUN apt-get install -y libxext6 libxtst6 libxi6 xauth

COPY scripts/build.sh $CONTMNTSCRIPTS/.
COPY scripts/create-user.sh $CONTMNTSCRIPTS/.
COPY scripts/display-runtime.sh $CONTMNTSCRIPTS/.
COPY diffconfig $CONTMNTSANDBOX/.

# Finished!
RUN echo 'Container is ready, run it using $ docker run -d --name openwrtapu4buildenv -v scripts:/mnt/docker/scripts -t t_openwrtapu4buildenv:latest'
RUN echo 'Attach to the container by running $ docker exec -it openwrtapu4buildenv bash'
RUN echo 'Create your user by running $ /mnt/docker/scripts/create_user.sh -u <USERNAME> -p <PASSWORD>'
RUN echo 'Switch to your user by running $ su <USERNAME>'
RUN echo 'Run the build script via $ /mnt/docker/scripts/build.sh'
RUN echo 'Binary will be located in /mnt/docker/sandbox/openwrt/bin/targets/x86/64'
