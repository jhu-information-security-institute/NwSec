# Description:
#   This Dockerfile creates a container with a build environment for the Brostrend ACL1 wireless USB adapter.
# Usage:
#   From this directory, run $ docker build -t brostrendbuildenv .
# Useful links:
#   https://www.kali.org/docs/development/recompiling-the-kali-linux-kernel/
#   https://deb.trendtechcn.com/
#   https://deb.trendtechcn.com/rtl88x2bu-dkms.deb 
# Hints:
#   You might need to upgrade your kernel.  E.g., 
#   # sudo apt-cache search linux-image
#   # apt install linux-image-5.5.0-kali2-amd64
#   # apt-get install linux-headers-5.5.0-kali2-amd64

# By default, runs as root

# List of containers for kali are here:
# https://gitlab.com/kalilinux/build-scripts/kali-docker/container_registry

# Pull kali:latest (not kali-rolling:latest)
FROM registry.gitlab.com/kalilinux/build-scripts/kali-docker/kali:2020.3

COPY sources.list /etc/apt/.

RUN apt-get update

# Get the dependencies for git, then get openssl
RUN apt-get install build-essential linux-headers-amd64 libncurses5-dev fakeroot wget dpkg-dev usbutils xz-utils net-tools network-manager iw iproute2 wireless-tools kmod rfkill dkms vim -y

# Finished!
RUN echo 'Container is ready, run it using $ docker run -it brostrendbuildenv:latest'
RUN echo 'Then, inside the running container, run: $ wget deb.trendtechcn.com/install -O /tmp/install && sh /tmp/install'
RUN echo 'After it builds, copy 88x2bu.ko from the container, by running (outside the container): $ docker cp brostrendbuildenvcontainer:/lib/modules/5.7.0-kali1-amd64/updates/dkms/88x2bu.ko .

