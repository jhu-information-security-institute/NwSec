# Description:
#   This Dockerfile creates a container with a build environment
# Usage:
#   From this directory, run $ sudo docker build -t tkalibuildenv .
# Useful websites:
#   https://www.kali.org/docs/development/recompiling-the-kali-linux-kernel/

# By default, runs as root

# List of containers for kali are here:
# https://gitlab.com/kalilinux/build-scripts/kali-docker/container_registry

# Pull kali:2019.4:latest
FROM registry.gitlab.com/kalilinux/build-scripts/kali-docker/kali:2019.4

RUN apt-get update

# Get the dependencies for git, then get openssl
RUN apt-get install build-essential linux-headers-amd64 libncurses5-dev fakeroot \
  wget dpkg-dev usbutils xz-utils -y

# Finished!
RUN echo 'Container is ready, run it using $ sudo docker --name kalibuildenv run -it tkalibuildenv:latest'
